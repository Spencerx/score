// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionTransaction.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/scenario/time_event.hpp>

#include <wobjectimpl.h>

#include <exception>
W_OBJECT_IMPL(Execution::EventComponent)

namespace Execution
{
EventComponent::EventComponent(
    const Scenario::EventModel& element, const Execution::Context& ctx, QObject* parent)
    : Execution::Component{ctx, "Executor::Event", nullptr}
    , m_score_event{&element}
{
  con(element, &Scenario::EventModel::conditionChanged, this, [this](const auto& expr) {
    auto exp_ptr = std::make_shared<ossia::expression_ptr>(this->makeExpression());
    this->in_exec(
        [e = m_ossia_event, exp_ptr] { e->set_expression(std::move(*exp_ptr)); });
  });
}

void EventComponent::cleanup(const std::shared_ptr<EventComponent>& self)
{
  in_exec([self, ev = m_ossia_event] {
    ev->cleanup();
    // And then we want it to be cleared in the main thread
    self->in_edit(gc(self));
  });
  m_ossia_event.reset();
}

ossia::expression_ptr EventComponent::makeExpression() const
{
  if(m_score_event)
  {
    try
    {
      return Engine::score_to_ossia::condition_expression(
          m_score_event->condition(), *system().execState);
    }
    catch(std::exception& e)
    {
      ossia::logger().error(e.what());
    }
  }
  return ossia::expressions::make_expression_true();
}

void EventComponent::onSetup(
    std::shared_ptr<ossia::time_event> event, ossia::expression_ptr expr,
    ossia::time_event::offset_behavior b)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  m_ossia_event = event;
  m_ossia_event->set_expression(std::move(expr));
  m_ossia_event->set_offset_behavior(b);
}

std::shared_ptr<ossia::time_event> EventComponent::OSSIAEvent() const
{
  return m_ossia_event;
}
}
