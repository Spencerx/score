#include "AudioApplicationPlugin.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Application/ScenarioActions.hpp>

#include <Audio/AudioDevice.hpp>
#include <Audio/AudioInterface.hpp>
#include <Audio/AudioTick.hpp>
#include <Audio/Settings/Model.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/audio/audio_engine.hpp>
#include <ossia/audio/audio_protocol.hpp>

#include <QToolBar>

SCORE_DECLARE_ACTION(RestartAudio, "Restart Audio", Common, QKeySequence::UnknownKey)
namespace Audio
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  startTimer(50);
}

void ApplicationPlugin::initialize()
{
  auto& set = context.settings<Audio::Settings::Model>();

  // First validate the current audio settings

  auto& engines = score::GUIAppContext().interfaces<Audio::AudioFactoryList>();

  if(auto dev = engines.get(set.getDriver()))
  {
    dev->initialize(set, this->context);
  }

  con(set, &Audio::Settings::Model::changed, this, &ApplicationPlugin::restart_engine,
      Qt::QueuedConnection);
}

ApplicationPlugin::~ApplicationPlugin() { }

void ApplicationPlugin::on_closeDocument(score::Document& old)
{
  stop_engine();
}

void ApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newdoc)
{
  if(newdoc)
    restart_engine();
}

void ApplicationPlugin::timerEvent(QTimerEvent*)
{
  if(audio)
    audio->gc();

  for(auto it = previous_audio.begin(); it != previous_audio.end();)
  {
    auto& engine = **it;
    engine.gc();
    if(engine.stop_received)
    {
      it = previous_audio.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  GUIElements e;

  auto& toolbars = e.toolbars;

  // The toolbar with the volume control
  m_audioEngineAct = new QAction{tr("Restart Audio"), this};
  m_audioEngineAct->setCheckable(true);
  m_audioEngineAct->setChecked(bool(audio));
  score::setHelp(m_audioEngineAct, "Restart the audio engine");

  setIcons(
      m_audioEngineAct, QStringLiteral(":/icons/engine_on.png"),
      QStringLiteral(":/icons/engine_off.png"),
      QStringLiteral(":/icons/engine_disabled.png"), false);
  {
    auto bar = new QToolBar(tr("Volume"));
    auto sl = new score::VolumeSlider{bar};
    sl->setFixedSize(100, 20);
    sl->setValue(0.5);
    score::setHelp(sl, "Change the master volume");
    bar->addWidget(sl);
    bar->addAction(m_audioEngineAct);
    connect(sl, &score::VolumeSlider::valueChanged, this, [this](double v) {
      if(!this->audio)
        return;

      auto doc = context.currentDocument();
      if(!doc)
        return;
      auto dev = (Dataflow::AudioDevice*)doc->plugin<Explorer::DeviceDocumentPlugin>()
                     .list()
                     .audioDevice();
      if(!dev)
        return;
      auto p = dev->getProtocol();
      if(!p)
        return;

      auto root = ossia::net::find_node(dev->getDevice()->get_root_node(), "/out/main");
      if(root)
      {
        if(auto p = root->get_parameter())
        {
          auto audio_p = static_cast<ossia::audio_parameter*>(p);
          audio_p->push_value(v);
        }
      }
    });

    toolbars.emplace_back(
        bar, StringKey<score::Toolbar>("Audio"), Qt::BottomToolBarArea, 400);
  }

  e.actions.container.reserve(2);
  e.actions.add<Actions::RestartAudio>(m_audioEngineAct);

  connect(
      m_audioEngineAct, &QAction::triggered, this, &ApplicationPlugin::restart_engine);

  return e;
}

void ApplicationPlugin::restart_engine()
try
{
  if(m_updating_audio)
    return;

#if defined(__EMSCRIPTEN__)
  static bool init = 0;
  if(!init)
  {
    init = true;
    start_engine();
  }
#else
  stop_engine();
  start_engine();
#endif
}
catch(std::exception& e)
{
  score::warning(
      context.documentTabWidget, tr("Audio Error"),
      tr("Warning: error while restarting the audio engine:\n%1").arg(e.what()));
}
catch(...)
{
  score::warning(
      context.documentTabWidget, tr("Audio Error"),
      tr("Warning: audio engine stuck. "
         "Operation aborted. "
         "Check the audio settings."));
}

void ApplicationPlugin::stop_engine()
{
  if(audio)
  {
    for(auto d : context.docManager.documents())
    {
      auto dev = (Dataflow::AudioDevice*)d->context()
                     .plugin<Explorer::DeviceDocumentPlugin>()
                     .list()
                     .audioDevice();
      if(dev)
      {
        if(auto proto = dev->getProtocol())
        {
          proto->stop();
        }
      }
    }

    audio->stop();
    previous_audio.push_back(std::move(audio));
    audio.reset();
  }
}

void ApplicationPlugin::start_engine()
{
  if(auto doc = this->currentDocument())
  {
    auto dev = (Dataflow::AudioDevice*)doc->context()
                   .plugin<Explorer::DeviceDocumentPlugin>()
                   .list()
                   .audioDevice();
    if(!dev)
      return;

    auto& set = this->context.settings<Audio::Settings::Model>();
    auto& engines = score::GUIAppContext().interfaces<Audio::AudioFactoryList>();

    if(auto dev = engines.get(set.getDriver()))
    {
      try
      {
        SCORE_ASSERT(!audio);
        audio = dev->make_engine(set, this->context);
        if(!audio)
          throw std::runtime_error{""};

        m_updating_audio = true;
        auto bs = audio->effective_buffer_size;
        if(bs <= 0)
          bs = set.getBufferSize();
        if(bs <= 0)
          bs = 512;
        auto rate = audio->effective_sample_rate;
        if(rate <= 0)
          rate = set.getRate();
        if(rate <= 0)
          rate = 44100;
        set.setBufferSize(bs);
        set.setRate(rate);
        m_updating_audio = false;
      }
      catch(...)
      {
        score::warning(
            nullptr, tr("Audio error"),
            tr("The desired audio settings could not be applied.\nPlease "
               "change "
               "them."));
      }
    }

    if(m_audioEngineAct)
      m_audioEngineAct->setChecked(bool(audio));

    dev->reconnect();
    if(audio)
    {
      audio->set_tick(Audio::makePauseTick(this->context));
    }
  }
}

}
