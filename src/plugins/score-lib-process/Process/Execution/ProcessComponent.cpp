// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessComponent.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <ossia/detail/thread.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Execution::ProcessComponent)

namespace Execution
{
ProcessComponent::~ProcessComponent() = default;
ProcessComponentFactory::~ProcessComponentFactory() = default;
ProcessComponentFactoryList::~ProcessComponentFactoryList() = default;

void ProcessComponent::lazy_init() { }

ProcessComponent::ProcessComponent(
    Process::ProcessModel& proc, const Context& ctx, const QString& name,
    QObject* parent)
    : Process::GenericProcessComponent<const Context>{proc, ctx, name, parent}
{
}

void ProcessComponent::cleanup()
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  if(const auto& proc = m_ossia_process)
  {
    this->system().setup.unregister_node(process(), proc->node);
    in_exec([proc] {
      OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
      proc->node.reset();
    });
  }
  node.reset();
  m_ossia_process.reset();
}
}
