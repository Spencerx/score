// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "BaseScenarioComponent.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
#include <ossia/editor/state/state.hpp>

#include <ossia-qt/invoke.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Execution::BaseScenarioElement)

namespace Execution
{
BaseScenarioElement::BaseScenarioElement(const Context& ctx, QObject* parent)
    : QObject{nullptr}
    , m_ctx{ctx}
{
}

BaseScenarioElement::~BaseScenarioElement() { }

struct FinishCallback final : public ossia::time_sync_callback
{
  explicit FinishCallback(BaseScenarioElement& s)
      : self{&s}
  {
  }

  void finished_evaluation(bool) override
  {
    ossia::qt::run_async(self.data(), [s = self] {
      if(s)
        s->finished();
    });
  }

  QPointer<BaseScenarioElement> self;
};

void BaseScenarioElement::init(bool forcePlay, BaseScenarioRefContainer element)
{
  if(element.interval().outlet && !element.interval().outlet->address().isSet())
  {

    QString audio_dev_name;
    for(auto dev : m_ctx.execState->edit_devices())
    {
      if(dynamic_cast<ossia::audio_protocol*>(&dev->get_protocol()))
      {
        audio_dev_name = QString::fromStdString(dev->get_name());
        break;
      }
    }
    if(!audio_dev_name.isEmpty())
      element.interval().outlet->setAddress(
          State::AddressAccessor{{audio_dev_name, {"out", "main"}}});
  }
  m_ossia_scenario = std::make_shared<ossia::scenario>();

  auto main_start_node = m_ossia_scenario->get_start_time_sync();
  auto main_end_node = std::make_shared<ossia::time_sync>();
  main_end_node->callbacks.callbacks.push_back(new FinishCallback{*this});
  m_ossia_scenario->add_time_sync(main_end_node);

  auto main_start_event = *main_start_node->emplace(
      main_start_node->get_time_events().begin(), [](auto&&...) {},
      ossia::expressions::make_expression_true());
  auto main_end_event = *main_end_node->emplace(
      main_end_node->get_time_events().begin(), [](auto&&...) {},
      ossia::expressions::make_expression_true());

  // TODO PlayDuration of base interval.
  // TODO PlayDuration of FullView
  auto main_interval = ossia::time_interval::create(
      {}, *main_start_event, *main_end_event,
      m_ctx.time(element.interval().duration.defaultDuration()),
      m_ctx.time(element.interval().duration.minDuration()),
      m_ctx.time(element.interval().duration.maxDuration()));
  m_ossia_scenario->add_time_interval(main_interval);

  m_ossia_startTimeSync
      = std::make_shared<TimeSyncComponent>(element.startTimeSync(), m_ctx, this);
  m_ossia_endTimeSync
      = std::make_shared<TimeSyncComponent>(element.endTimeSync(), m_ctx, this);

  m_ossia_startEvent
      = std::make_shared<EventComponent>(element.startEvent(), m_ctx, this);
  m_ossia_endEvent = std::make_shared<EventComponent>(element.endEvent(), m_ctx, this);

  m_ossia_startState = std::make_shared<StateComponent>(
      element.startState(), main_start_event, m_ctx, this);
  m_ossia_endState = std::make_shared<StateComponent>(
      element.endState(), main_end_event, m_ctx, this);

  m_ossia_interval = std::make_shared<IntervalComponent>(
      element.interval(), std::shared_ptr<ossia::scenario>{}, m_ctx, this);

  if(forcePlay)
  {
    m_ossia_startTimeSync->onSetup(
        main_start_node, ossia::expressions::make_expression_true());
    m_ossia_endTimeSync->onSetup(
        main_end_node, ossia::expressions::make_expression_false());
  }
  else
  {
    m_ossia_startTimeSync->onSetup(
        main_start_node, m_ossia_startTimeSync->makeTrigger());
    m_ossia_endTimeSync->onSetup(main_end_node, m_ossia_endTimeSync->makeTrigger());
  }

  m_ossia_startEvent->onSetup(
      main_start_event, m_ossia_startEvent->makeExpression(),
      (ossia::time_event::offset_behavior)element.startEvent().offsetBehavior());
  m_ossia_endEvent->onSetup(
      main_end_event, m_ossia_endEvent->makeExpression(),
      (ossia::time_event::offset_behavior)element.endEvent().offsetBehavior());

  main_start_node->set_start(true);

  // Important: we do not setup the end state in order to not have it
  // send its messages twice as this is already handled elsewhere in score.
  // (when we press stop manually)
  // This should be refactored though, for now it's a bit ugly.
  // m_ossia_endState->onSetup();
  m_ossia_interval->onSetup(
      m_ossia_interval, main_interval, m_ossia_interval->makeDurations(), true);

  m_ossia_scenario->start();
}

void BaseScenarioElement::cleanup()
{
  if(m_ossia_interval)
    m_ossia_interval->cleanup(m_ossia_interval);
  if(m_ossia_startState)
    m_ossia_startState->cleanup(m_ossia_startState);
  if(m_ossia_endState)
    m_ossia_endState->cleanup(m_ossia_startState);
  if(m_ossia_startEvent)
    m_ossia_startEvent->cleanup(m_ossia_startEvent);
  if(m_ossia_endEvent)
    m_ossia_endEvent->cleanup(m_ossia_endEvent);
  if(m_ossia_startTimeSync)
    m_ossia_startTimeSync->cleanup(m_ossia_startTimeSync);
  if(m_ossia_endTimeSync)
    m_ossia_endTimeSync->cleanup(m_ossia_endTimeSync);
  m_ossia_interval.reset();
  m_ossia_startState.reset();
  m_ossia_endState.reset();
  m_ossia_startEvent.reset();
  m_ossia_endEvent.reset();
  m_ossia_startTimeSync.reset();
  m_ossia_endTimeSync.reset();
  m_ossia_scenario.reset();
}

ossia::scenario& BaseScenarioElement::baseScenario() const
{
  return *m_ossia_scenario;
}

IntervalComponent& BaseScenarioElement::baseInterval() const
{
  return *m_ossia_interval;
}

TimeSyncComponent& BaseScenarioElement::startTimeSync() const
{
  return *m_ossia_startTimeSync;
}

TimeSyncComponent& BaseScenarioElement::endTimeSync() const
{
  return *m_ossia_endTimeSync;
}

EventComponent& BaseScenarioElement::startEvent() const
{
  return *m_ossia_startEvent;
}

EventComponent& BaseScenarioElement::endEvent() const
{
  return *m_ossia_endEvent;
}

StateComponent& BaseScenarioElement::startState() const
{
  return *m_ossia_startState;
}

StateComponent& BaseScenarioElement::endState() const
{
  return *m_ossia_endState;
}
}

BaseScenarioRefContainer::BaseScenarioRefContainer(
    Scenario::IntervalModel& interval, Scenario::ScenarioInterface& s)
    : m_interval{interval}
    , m_startState{s.state(interval.startState())}
    , m_endState{s.state(interval.endState())}
    , m_startEvent{s.event(m_startState.eventId())}
    , m_endEvent{s.event(m_endState.eventId())}
    , m_startNode{s.timeSync(m_startEvent.timeSync())}
    , m_endNode{s.timeSync(m_endEvent.timeSync())}
{
}
