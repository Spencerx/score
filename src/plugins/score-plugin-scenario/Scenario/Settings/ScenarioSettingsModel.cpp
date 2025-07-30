// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioSettingsModel.hpp"

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/File.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
namespace Scenario
{
namespace Settings
{
namespace Parameters
{
SETTINGS_PARAMETER_IMPL(Skin){QStringLiteral("Skin/Skin"), "Default"};
SETTINGS_PARAMETER_IMPL(DefaultEditor){QStringLiteral("Skin/DefaultEditor"), ""};
SETTINGS_PARAMETER_IMPL(GraphicZoom){QStringLiteral("Skin/Zoom"), 1};
SETTINGS_PARAMETER_IMPL(SlotHeight){QStringLiteral("Skin/slotHeight"), 200};
SETTINGS_PARAMETER_IMPL(DefaultDuration){
    QStringLiteral("Skin/defaultDuration"), TimeVal::fromMsecs(15000)};
SETTINGS_PARAMETER_IMPL(SnapshotOnCreate){
    QStringLiteral("Scenario/SnapshotOnCreate"), false};
SETTINGS_PARAMETER_IMPL(AutoSequence){QStringLiteral("Scenario/AutoSequence"), false};
SETTINGS_PARAMETER_IMPL(TimeBar)
{
  QStringLiteral("Scenario/TimeBar"),
#if defined(__EMSCRIPTEN__)
      false
#else
      true
#endif
};
SETTINGS_PARAMETER_IMPL(MeasureBars){QStringLiteral("Scenario/MeasureBars"), true};
SETTINGS_PARAMETER_IMPL(MagneticMeasures){
    QStringLiteral("Scenario/MagneticMeasures"), true};
SETTINGS_PARAMETER_IMPL(UpdateRate){QStringLiteral("Scenario/UpdateRate"), 60};
SETTINGS_PARAMETER_IMPL(ExecutionRefreshRate){
    QStringLiteral("Scenario/ExecutionRefreshRate"), 60};

static auto list()
{
  return std::tie(
      Skin, DefaultEditor, GraphicZoom, SlotHeight, DefaultDuration, SnapshotOnCreate,
      AutoSequence, TimeBar, MeasureBars, MagneticMeasures, UpdateRate,
      ExecutionRefreshRate);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);

  if(m_DefaultDuration < TimeVal::fromMsecs(1000))
    setDefaultDuration(TimeVal::fromMsecs(15000));
  if(m_DefaultDuration > TimeVal::fromMsecs(10000000))
    setDefaultDuration(TimeVal::fromMsecs(100000));
  // setDefaultDuration(TimeVal::fromMsecs(100000));

  bind(*this, Model::p_UpdateRate{}, this, [&ctx](int r) {
    auto& set = const_cast<score::ApplicationSettings&>(ctx.applicationSettings);
    set.uiEventRate = r;
    for(auto& doc : ctx.documents.documents())
    {
      doc->updateTimers();
    }
  });
}

QString Model::getSkin() const
{
  return m_Skin;
}

void Model::initSkin(const QString& skin)
{
  m_Skin = skin;

  if(!score::AppContext().applicationSettings.gui)
    return;

  QFile f(skin);
  if(skin.isEmpty() || skin == "Default" || !f.exists())
    f.setFileName(":/skin/DefaultSkin.json");

  if(f.open(QFile::ReadOnly))
  {
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(score::mapAsByteArray(f), &err);
    if(err.error)
    {
      qDebug() << "could not load skin : " << err.errorString() << err.offset;
    }
    else
    {
      auto obj = doc.object();
      score::Skin::instance().load(obj);
    }
  }
  else
  {
    qDebug() << "could not open" << f.fileName();
  }

  SkinChanged(skin);
}

void Model::setSkin(const QString& skin)
{
  if(m_Skin == skin)
    return;

  initSkin(skin);

  QSettings s;
  s.setValue(Parameters::Skin.key, m_Skin);
}

TimeVal Model::getDefaultDuration() const
{
  return m_DefaultDuration;
}

void Model::initDefaultDuration(TimeVal val)
{
  val = std::max(val, TimeVal::fromMsecs(100));

  if(val == m_DefaultDuration)
    return;

  m_DefaultDuration = val;

  DefaultDurationChanged(val);
}
void Model::setDefaultDuration(TimeVal val)
{
  initDefaultDuration(val);

  QSettings s;
  s.setValue(Parameters::DefaultDuration.key, QVariant::fromValue(m_DefaultDuration));
}

SCORE_SETTINGS_PARAMETER_CPP(double, Model, GraphicZoom)
SCORE_SETTINGS_PARAMETER_CPP(qreal, Model, SlotHeight)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, SnapshotOnCreate)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, DefaultEditor)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, AutoSequence)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, TimeBar)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, MeasureBars)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, MagneticMeasures)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, UpdateRate)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, ExecutionRefreshRate)
}

double getNewLayerHeight(
    const score::ApplicationContext& ctx, const Process::ProcessModel& proc) noexcept
{
  double h = -1.;
  if(const auto& fact
     = ctx.interfaces<Process::LayerFactoryList>().get(proc.concreteKey()))
  {
    if(auto opt_h = fact->recommendedHeight())
    {
      h = *opt_h;
    }
  }

  if(h == -1.)
  {
    h = ctx.settings<Scenario::Settings::Model>().getSlotHeight();
  }
  return h;
}

}
