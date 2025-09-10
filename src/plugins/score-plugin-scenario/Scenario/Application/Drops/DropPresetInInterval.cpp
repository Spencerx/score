#include <Process/ProcessList.hpp>

#include <Scenario/Application/Drops/DropPresetInInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace Scenario
{

void DropPresetInInterval::perform(
    const IntervalModel& interval, const score::DocumentContext& ctx,
    Scenario::Command::Macro& m, const QByteArray& presetData, QPointF pos)
{
  auto& procs = ctx.app.interfaces<Process::ProcessFactoryList>();
  if(auto preset = Process::Preset::fromJson(procs, presetData))
  {
    // Finally we show the newly created rack
    m.loadProcessFromPreset(interval, *preset, pos);
    m.showRack(interval);
  }
}

bool DropPresetInInterval::drop(
    const score::DocumentContext& ctx, const IntervalModel& interval, QPointF pos,
    const QMimeData& mime)
{
  if(mime.hasFormat(score::mime::processpreset()))
  {
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};

    const auto json = mime.data(score::mime::processpreset());
    perform(interval, ctx, m, json, pos);
    m.commit();
    return true;
  }
  else if(mime.hasUrls())
  {
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};
    bool ok = false;
    for(const QUrl& u : mime.urls())
    {
      auto path = u.toLocalFile();
      if(QFile f{path}; QFileInfo{f}.suffix() == "scp" && f.open(QIODevice::ReadOnly))
      {
        ok = true;
        perform(interval, ctx, m, score::mapAsByteArray(f), pos);
      }
    }

    if(ok)
    {
      m.commit();
      return true;
    }
  }

  return false;
}

}
