#include "DropPortInScenario.hpp"

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <Dataflow/PortItem.hpp>
namespace Dataflow
{
DropPortInScenario::DropPortInScenario() { }

bool DropPortInScenario::canDrop(const QMimeData& mime) const noexcept
{
  if(mime.hasFormat(score::mime::port()))
  {
    auto base_port = Dataflow::PortItem::clickedPort;
    if(!base_port || base_port->port().type() != Process::PortType::Message
       || qobject_cast<const Process::Outlet*>(&base_port->port()))
      return false;

    return bool(dynamic_cast<Dataflow::AutomatablePortItem*>(base_port));
  }
  return false;
}

bool DropPortInScenario::drop(
    const Scenario::ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if(mime.hasFormat(score::mime::port()))
  {
    auto base_port = Dataflow::PortItem::clickedPort;
    if(!base_port || base_port->port().type() != Process::PortType::Message
       || qobject_cast<const Process::Outlet*>(&base_port->port()))
      return false;

    auto port = dynamic_cast<Dataflow::AutomatablePortItem*>(base_port);
    if(!port)
      return false;

    Scenario::Command::Macro m{
        new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    // 5 seconds.
    // TODO instead use a percentage of the currently displayed view
    TimeVal t = TimeVal::fromMsecs(5000);

    auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

    // Create process
    auto ok = port->on_createAutomation(
        interval, [&](score::Command* cmd) { m.submit(cmd); }, pres.context().context);
    if(!ok)
    {
      return false;
    }

    m.showRack(interval);

    m.commit();
    return true;
  }

  return false;
}

bool DropPortInInterval::drop(
    const score::DocumentContext& ctx, const Scenario::IntervalModel& interval,
    QPointF p, const QMimeData& mime)
{
  if(mime.hasFormat(score::mime::port()))
  {
    auto base_port = Dataflow::PortItem::clickedPort;
    if(!base_port || base_port->port().type() != Process::PortType::Message
       || qobject_cast<const Process::Outlet*>(&base_port->port()))
      return false;

    auto port = dynamic_cast<Dataflow::AutomatablePortItem*>(base_port);
    if(!port)
      return false;

    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};
    auto ok = port->on_createAutomation(
        interval, [&](score::Command* cmd) { m.submit(cmd); }, ctx);
    if(!ok)
    {
      return false;
    }

    m.showRack(interval);
    m.commit();
    return true;
  }

  return false;
}

}
