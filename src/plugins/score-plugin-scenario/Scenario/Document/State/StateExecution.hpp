#pragma once
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionComponent.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Components/IntervalComponent.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/model/ComponentHierarchy.hpp>

#include <ossia/dataflow/nodes/state.hpp>

namespace ossia
{
class graph_node;
class time_event;
}
namespace Scenario
{
class StateModel;
}

namespace Execution
{
class SCORE_PLUGIN_SCENARIO_EXPORT StateComponentBase : public Execution::Component
{
  COMMON_COMPONENT_METADATA("b3905e79-2bd0-48bd-8654-8666455ceedd")
public:
  static constexpr bool is_unique = true;

  using parent_t = Execution::Component;
  using model_t = Process::ProcessModel;
  using component_t = ProcessComponent;
  using component_factory_list_t = ProcessComponentFactoryList;
  StateComponentBase(
      const Scenario::StateModel& element, std::shared_ptr<ossia::time_event> ev,
      const Execution::Context& ctx, QObject* parent);

  //! To be called from the GUI thread
  void onDelete() const;
  void updateControls();

  ProcessComponent*
  make(ProcessComponentFactory& factory, Process::ProcessModel& process);
  ProcessComponent* make(Process::ProcessModel& process) { return nullptr; }
  std::function<void()> removing(const Process::ProcessModel& e, ProcessComponent& c);

  template <typename... Args>
  void added(Args&&...)
  {
  }
  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if(f)
      f();
  }
  template <typename Models>
  const auto& models() const
  {
    static_assert(
        std::is_same<Models, Process::ProcessModel>::value,
        "State component must be passed Process::ProcessModel as child.");

    return m_model->stateProcesses;
  }

  const auto& processes() const { return m_processes; }
  const auto& state() const
  {
    SCORE_ASSERT(m_model);
    return *m_model;
  }
  const std::shared_ptr<ossia::nodes::state_writer>& node() const { return m_node; }

protected:
  QPointer<const Scenario::StateModel> m_model;
  std::shared_ptr<ossia::time_event> m_ev;
  std::shared_ptr<ossia::nodes::state_writer> m_node;
  score::hash_map<Id<Process::ProcessModel>, std::shared_ptr<ProcessComponent>>
      m_processes;
};

class SCORE_PLUGIN_SCENARIO_EXPORT StateComponent final
    : public score::PolymorphicComponentHierarchy<StateComponentBase, false>
{
public:
  template <typename... Args>
  StateComponent(Args&&... args)
      : PolymorphicComponentHierarchyManager{
          score::lazy_init_t{}, std::forward<Args>(args)...}
  {
    init();
  }

  StateComponent(const StateComponent&) = delete;
  StateComponent(StateComponent&&) = delete;
  StateComponent& operator=(const StateComponent&) = delete;
  StateComponent& operator=(StateComponent&&) = delete;
  ~StateComponent();

  void init();
  void cleanup(const std::shared_ptr<StateComponent>&);
};
}
