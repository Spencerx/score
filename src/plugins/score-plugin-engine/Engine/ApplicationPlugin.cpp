//// This is an open source non-commercial project. Dear PVS-Studio, please
/// check / it. PVS-Studio Static Code Analyzer for C, C++ and C#:
/// http://www.viva64.com
#include "ApplicationPlugin.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Settings/ExplorerModel.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Inspector/Interval/SpeedSlider.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/ToolbarManager.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/TimeSpinBox.hpp>

#include <Process/ApplicationPlugin.hpp>
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia-qt/invoke.hpp>

#include <QLabel>
#include <QMainWindow>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>

#include <wobjectimpl.h>

namespace Engine
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
    , m_playActions{*this, ctx}
    , m_execution{ctx}
{
  if(ctx.applicationSettings.gui)
  {
    auto& ctrl = ctx
            .applicationPlugin<Process::ApplicationPlugin>();
    m_playActions.setupContextMenu(ctrl.layerContextMenuRegistrar());
  }
}

ApplicationPlugin::~ApplicationPlugin()
{
  // The scenarios playing should already have been stopped by virtue of
  // aboutToClose.
}

void ApplicationPlugin::afterStartup()
{
  if(!context.documents.documents().empty())
  {
    if(context.applicationSettings.autoplay)
    {
      // TODO what happens if we load multiple documents ?
      QTimer::singleShot(
          (1 + context.applicationSettings.waitAfterLoad) * 1000, &m_execution,
          [this] { m_execution.request_play_local(true); });
    }
  }
}

void ApplicationPlugin::initialize()
{
  // Update the clock widget
  // See TransportActions::makeGUIElements
  auto& toolbars = this->context.toolbars.get();
  auto transport_toolbar = toolbars.find(StringKey<score::Toolbar>("Transport"));
  if(transport_toolbar != toolbars.end())
  {
    const auto& tb = transport_toolbar->second.toolbar();
    if(tb)
    {
      auto cld = tb->children();
      if(!cld.empty())
      {
        auto label = tb->findChild<QLabel*>("TimeLabel");
        if(label)
          setupTimingWidget(label);

        m_speedSlider = dynamic_cast<Scenario::SpeedWidget*>(cld.back());
      }
    }
  }

  if(m_musicalAct)
  {
    auto& settings = this->context.settings<Scenario::Settings::Model>();
    m_musicalAct->setChecked(settings.getMeasureBars());

    score::setGlobalTimeMode(
        settings.getMeasureBars() ? score::TimeMode::Bars : score::TimeMode::Seconds);

    connect(m_musicalAct, &QAction::toggled, this, [this](bool ok) {
      auto& settings = this->context.settings<Scenario::Settings::Model>();
      settings.setMeasureBars(ok);
      settings.setMagneticMeasures(ok);
      score::setGlobalTimeMode(ok ? score::TimeMode::Bars : score::TimeMode::Seconds);

      //if (auto doc = this->currentDocument())
      //{
      //  auto& mod = doc->model().modelDelegate();
      //  auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
      //  if (scenar)
      //  {
      //    auto& itv = scenar->baseInterval();
      //    itv.setHasTimeSignature(ok);
      //  }
      //}
    });
  }

  m_execution.init_transport();
}

QWidget* ApplicationPlugin::setupTimingWidget(QLabel* time_label)
{
  score::setHelp(time_label, tr("Elapsed time since the beginning of playback"));

  auto c = connect(
      &execution_ui_clock_timer, &QTimer::timeout, time_label, [this, time_label] {
    auto t = m_execution.execution_time();
    if(t == TimeVal::zero())
    {
      time_label->setText(QStringLiteral("00:00:00.000"));
    }
    else
    {
      const double one_hour_in_ms = 3600 * 1000;
      const double one_minute_in_ms = 60000;
      const double one_second_in_ms = 1000;
      int64_t ms = t.impl / ossia::flicks_per_millisecond<double>;
      int64_t hours = ms / one_hour_in_ms;
      ms -= hours * one_hour_in_ms;
      int64_t minutes = ms / one_minute_in_ms;
      ms -= minutes * one_minute_in_ms;
      int64_t seconds = ms / one_second_in_ms;
      ms -= seconds * one_second_in_ms;

      time_label->setText(QStringLiteral("%1:%2:%3.%4")
                              .arg(hours, 2, 10, QLatin1Char('0'))
                              .arg(minutes, 2, 10, QLatin1Char('0'))
                              .arg(seconds, 2, 10, QLatin1Char('0'))
                              .arg(ms, 3, 10, QLatin1Char('0')));
    }
  });
  execution_ui_clock_timer.start(1000 / 20);
  return time_label;
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  GUIElements e;

  auto& toolbars = e.toolbars;

  toolbars.reserve(4);

  // The toolbar with the interval controls
  {
    auto ui_toolbar = new QToolBar(tr("Interval"));
    toolbars.emplace_back(
        ui_toolbar, StringKey<score::Toolbar>("UISetup"), Qt::BottomToolBarArea, 10);

    {
      auto timeline_act = new QAction{tr("Timeline interval"), ui_toolbar};
      timeline_act->setCheckable(true);
      timeline_act->setChecked(false);
      timeline_act->setShortcut(QKeySequence("Ctrl+Alt+N"));
      score::setHelp(timeline_act, tr("Change between nodal and timeline mode"));
      setIcons(
          timeline_act, QStringLiteral(":/icons/nodal_on.png"),
          QStringLiteral(":/icons/nodal_hover.png"),
          QStringLiteral(":/icons/nodal_on.png"),
          QStringLiteral(":/icons/nodal_disabled.png"));

      connect(timeline_act, &QAction::toggled, this, [timeline_act](bool checked) {
        if(!checked)
        {
          setIcons(
              timeline_act, QStringLiteral(":/icons/timeline_on.png"),
              QStringLiteral(":/icons/timeline_hover.png"),
              QStringLiteral(":/icons/timeline_on.png"),
              QStringLiteral(":/icons/timeline_disabled.png"));
        }
        else
        {
          setIcons(
              timeline_act, QStringLiteral(":/icons/nodal_on.png"),
              QStringLiteral(":/icons/nodal_hover.png"),
              QStringLiteral(":/icons/nodal_on.png"),
              QStringLiteral(":/icons/nodal_disabled.png"));
        }
      });
      ui_toolbar->addAction(timeline_act);
    }

    {
      m_musicalAct = new QAction{tr("Enable musical mode"), this};
      m_musicalAct->setCheckable(true);
      score::setHelp(m_musicalAct, tr("Enable musical mode"));
      setIcons(
          m_musicalAct, QStringLiteral(":/icons/music_on.png"),
          QStringLiteral(":/icons/music_hover.png"),
          QStringLiteral(":/icons/music_off.png"),
          QStringLiteral(":/icons/music_disabled.png"));

      ui_toolbar->addAction(m_musicalAct);
    }

    {
      auto show_cables_act = context.actions.action<Actions::ShowCables>();
      ui_toolbar->addAction(show_cables_act.action());
    }
  }

  return e;
}

void ApplicationPlugin::on_initDocument(score::Document& doc)
{
  score::addDocumentPlugin<LocalTree::DocumentPlugin>(doc);
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  LocalTree::DocumentPlugin* lt = doc.context().findPlugin<LocalTree::DocumentPlugin>();
  if(lt)
  {
    lt->init();
    initLocalTreeNodes(*lt);
  }
  score::addDocumentPlugin<Execution::DocumentPlugin>(doc);
}

void ApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newdoc)
{
  if(olddoc)
  {
    // Disable the local tree for this document by removing
    // the node temporarily
    /*
    auto& doc_plugin = olddoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(false);
    */

    if(m_speedSlider)
      m_speedSlider->unsetInterval();
    // TODO check whether the widget gets deleted
  }

  if(newdoc)
  {
    // Enable the local tree for this document.

    /*
    auto& doc_plugin = newdoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(true);
    */

    // Setup speed toolbar
    auto& root = score::IDocument::get<Scenario::ScenarioDocumentModel>(*newdoc);

    if(context.applicationSettings.gui)
    {
      if(m_speedSlider)
        m_speedSlider->setInterval(root.baseInterval());
    }

    // Setup audio & devices
    auto& doc_plugin = newdoc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* set = newdoc->context().findPlugin<Explorer::ProjectSettings::Model>();
    if(set)
    {
      if(set->getReconnectOnStart())
      {
        auto& list = doc_plugin.list();
        list.apply([&](Device::DeviceInterface& dev) {
          if(&dev != list.audioDevice() && &dev != list.localDevice())
            dev.reconnect();
        });

        if(set->getRefreshOnStart())
        {
          list.apply([&](Device::DeviceInterface& dev) {
            if(&dev != list.audioDevice() && &dev != list.localDevice())
              if(dev.connected())
              {
                auto old_name = dev.name();
                auto new_node = dev.refresh();

                auto& explorer = doc_plugin.explorer();
                const auto& cld = explorer.rootNode().children();
                for(auto it = cld.begin(); it != cld.end(); ++it)
                {
                  auto ds = it->get<Device::DeviceSettings>();
                  if(ds.name == old_name)
                  {
                    explorer.removeNode(it);
                    break;
                  }
                }

                explorer.addDevice(std::move(new_node));
              }
          });
        }
      }
    }
  }
}

void ApplicationPlugin::prepareNewDocument()
{
  execution().request_stop();
  // TODO what if JACK is stuck? force-stop after some time ?
}

void ApplicationPlugin::initLocalTreeNodes(LocalTree::DocumentPlugin& lt)
{
  auto& appplug = *this;
  auto& root = lt.device().get_root_node();

  {
    auto n = root.create_child("running");
    auto p = n->create_parameter(ossia::val_type::BOOL);
    p->set_value(false);
    p->set_access(ossia::access_mode::GET);

    if(context.applicationSettings.gui)
    {
      auto& play_action = appplug.context.actions.action<Actions::Play>();
      connect(
          play_action.action(), &QAction::toggled, &lt, [p] { p->push_value(true); });

      auto& stop_action = context.actions.action<Actions::Stop>();
      connect(
          stop_action.action(), &QAction::toggled, &lt, [p] { p->push_value(false); });
    }
  }
  {
    auto local_play_node = root.create_child("play");
    auto local_play_address = local_play_node->create_parameter(ossia::val_type::BOOL);
    local_play_address->set_value(bool{false});
    local_play_address->set_access(ossia::access_mode::SET);
    local_play_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [this, v] {
        if(auto val = v.target<bool>())
        {
          execution().request_play_from_localtree(*val);
        }
      });
    });
  }

  {
    auto local_play_node = root.create_child("global_play");
    auto local_play_address = local_play_node->create_parameter(ossia::val_type::BOOL);
    local_play_address->set_value(bool{false});
    local_play_address->set_access(ossia::access_mode::SET);
    local_play_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [this, v] {
        if(auto val = v.target<bool>())
        {
          execution().request_play_global_from_localtree(*val);
        }
      });
    });
  }

  {
    auto local_transport_node = root.create_child("transport");
    auto local_transport_address
        = local_transport_node->create_parameter(ossia::val_type::FLOAT);
    local_transport_address->set_value(bool{false});
    local_transport_address->set_access(ossia::access_mode::SET);
    local_transport_address->set_unit(ossia::millisecond_u{});
    local_transport_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [this, v] {
        execution().request_transport_from_localtree(
            TimeVal::fromMsecs(ossia::convert<float>(v)));
      });
    });
  }

  {
    auto local_stop_node = root.create_child("stop");
    auto local_stop_address
        = local_stop_node->create_parameter(ossia::val_type::IMPULSE);
    local_stop_address->set_value(ossia::impulse{});
    local_stop_address->set_access(ossia::access_mode::SET);
    local_stop_address->add_callback([&](const ossia::value&) {
      ossia::qt::run_async(this, [this] { execution().request_stop_from_localtree(); });
    });
  }

  {
    auto local_stop_node = root.create_child("reinit");
    auto local_stop_address
        = local_stop_node->create_parameter(ossia::val_type::IMPULSE);
    local_stop_address->set_value(ossia::impulse{});
    local_stop_address->set_access(ossia::access_mode::SET);
    local_stop_address->add_callback([&](const ossia::value&) {
      ossia::qt::run_async(
          this, [this] { execution().request_reinitialize_from_localtree(); });
    });
  }
  {
    auto node = root.create_child("exit");
    auto address = node->create_parameter(ossia::val_type::STRING);
    address->set_value(std::string{});
    address->set_access(ossia::access_mode::SET);
    address->add_callback([&](const ossia::value& v) {
      bool force = ossia::convert<std::string>(v) == "force";
      ossia::qt::run_async(this, [this, force] {
        execution().request_stop_from_localtree();

        QTimer::singleShot(500, [force] {
          if(force)
          {
            for(auto& doc : score::GUIAppContext().docManager.documents())
            {
              doc->commandStack().markCurrentIndexAsSaved();
            }
          }

          score::GUIApplicationInterface::instance().forceExit();
        });
      });
    });
  }

  {
    auto node = root.create_child("document");
    auto address = node->create_parameter(ossia::val_type::INT);
    address->set_value(0);
    address->set_access(ossia::access_mode::BI);
    address->add_callback([&](const ossia::value& v) {
      int val = v.get<int>();
      ossia::qt::run_async(this, [this, val] {
        if(context.applicationSettings.gui)
        {
          QTabWidget* docs = context.mainWindow->centralWidget()->findChild<QTabWidget*>(
              "Documents", Qt::FindDirectChildrenOnly);
          SCORE_ASSERT(docs);
          if(docs)
          {
            docs->setCurrentIndex(std::clamp(val, 0, docs->count() - 1));
          }
        }
      });
    });
  }

  {
    auto node = root.create_child("reconnect");
    auto address = node->create_parameter(ossia::val_type::STRING);
    address->set_value("");
    address->set_access(ossia::access_mode::BI);
    address->add_callback([&](const ossia::value& v) {
      auto val = QString::fromStdString(v.get<std::string>());
      ossia::qt::run_async(this, [this, device = std::move(val)] {
        auto doc = currentDocument();
        if(!doc)
          return;

        auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
        plug.reconnect(device);
      });
    });
  }
// FIXME
#if 0
  {
    auto settings = root.create_child("settings");
    auto audio = settings->create_child("audio");
    {
      {
        auto node = audio->create_child("backend");
        auto address = node->create_parameter(ossia::val_type::STRING);
        address->add_callback([&](const ossia::value& v) {
          QString val = QString::fromStdString(v.get<std::string>());
          ossia::qt::run_async(this, [this, val] {
            auto& set = score::AppContext().settings<Audio::Settings::Model>();
          });
        });
      }

      {
        auto node = audio->create_child("buffer_size");
        auto address = node->create_parameter(ossia::val_type::INT);
        address->add_callback([&](const ossia::value& v) {
          int val = v.get<int>();
          ossia::qt::run_async(this, [this, val] {
            auto& set = score::AppContext().settings<Audio::Settings::Model>();
            set.setBufferSize(val);
          });
        });
      }

      {
        auto node = audio->create_child("rate");
        auto address = node->create_parameter(ossia::val_type::INT);
        address->add_callback([&](const ossia::value& v) {
          int val = v.get<int>();
          ossia::qt::run_async(this, [this, val] {
            auto& set = score::AppContext().settings<Audio::Settings::Model>();
            set.setRate(val);
          });
        });
      }
    }
  }
#endif
}
}
