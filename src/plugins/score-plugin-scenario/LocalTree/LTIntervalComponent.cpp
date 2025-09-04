// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalComponent.hpp"

#include <State/Expression.hpp>

#include <Process/Dataflow/Port.hpp>

#include <ossia/detail/algorithms.hpp>

// TODO move this !
namespace State::convert
{
template <>
State::AddressAccessor value(const ossia::value& val)
{
  if(val.get_type() == ossia::val_type::STRING)
  {
    if(auto res
       = State::parseAddressAccessor(QString::fromStdString(*val.target<std::string>())))
      return *res;
  }
  return {};
}
template <>
ossia::value value(const ossia::value& val)
{
  return std::move(val);
}
const ossia::value& value(const ossia::value& val)
{
  return val;
}
ossia::value&& value(ossia::value&& val)
{
  return std::move(val);
}
}

namespace ossia
{
template <>
struct qt_property_converter<State::AddressAccessor>
{
  static constexpr const auto val = ossia::val_type::STRING;
  using type = std::string;
  static std::string convert(const State::AddressAccessor& t)
  {
    return t.toString().toStdString();
  }
  static std::string convert(State::AddressAccessor&& t)
  {
    return t.toString().toStdString();
  }
};

template <>
struct qt_property_converter<ossia::value>
{
  static constexpr const auto val = ossia::val_type::LIST;
  using type = ossia::value;
  static ossia::value convert(const ossia::value& t) { return t; }
  static ossia::value convert(ossia::value&& t) { return std::move(t); }
};
}

namespace LocalTree
{
template <typename Property, typename Object>
auto add_value_property(
    ossia::net::node_base& n, Object& obj, const std::string& name, QObject* context)
{
  auto node = n.create_child(name);
  SCORE_ASSERT(node);

  auto addr = node->create_parameter(obj.value().get_type());
  SCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::BI);
  return std::make_unique<PropertyWrapper<Property>>(*addr, obj, context);
}
class DefaultProcessComponent final : public ProcessComponent
{
  COMPONENT_METADATA("0b801b8f-41db-49ca-a396-c26aadf3f1b5")
public:
  DefaultProcessComponent(
      ossia::net::node_base& node, Process::ProcessModel& proc,
      const score::DocumentContext& doc, QObject* parent)
      : ProcessComponent{node, proc, doc, "ProcessComponent", parent}
  {
    recompute();

    connect(
        &proc, &Process::ProcessModel::controlAdded, this,
        &DefaultProcessComponent::recompute);
    connect(
        &proc, &Process::ProcessModel::controlRemoved, this,
        &DefaultProcessComponent::recompute);
    connect(
        &proc, &Process::ProcessModel::controlOutletAdded, this,
        &DefaultProcessComponent::recompute);
    connect(
        &proc, &Process::ProcessModel::controlOutletRemoved, this,
        &DefaultProcessComponent::recompute);
    connect(
        &proc, &Process::ProcessModel::inletsChanged, this,
        &DefaultProcessComponent::recompute);
    connect(
        &proc, &Process::ProcessModel::outletsChanged, this,
        &DefaultProcessComponent::recompute);
  }

  ~DefaultProcessComponent()
  {
    for(int i = 2, n = m_properties.size(); i < n; i++)
      m_properties[i].get()->clear();
    while(m_properties.size() > 2)
      m_properties.pop_back();
  }

  void recompute()
  {
    // important: we have to remove things in reverse order than they were added.
    // otherwise a parent property wrapper gets deleted before its children, which
    // will still try to access its parent in its own destructor.
    for(int i = 2, n = m_properties.size(); i < n; i++)
      m_properties[i].get()->clear();

    while(m_properties.size() > 2)
      m_properties.pop_back();
    auto& proc = process();
    for(auto& inlet : proc.inlets())
      if(auto control = qobject_cast<Process::ControlInlet*>(inlet))
        add_inlet(*control);

    for(auto& outlet : proc.outlets())
      if(auto control = qobject_cast<Process::ControlOutlet*>(outlet))
        add_outlet(*control);
  }

  void add_inlet(Process::ControlInlet& control)
  {
    if(!control.exposed().isEmpty())
    {
      try
      {
        m_properties.push_back(
            add_property<Process::Inlet::p_address>(
                this->node(), control, control.exposed().toStdString(), this));
        auto& port_node = m_properties.back()->addr.get_node();
        auto prop = add_value_property<Process::ControlInlet::p_value>(
            port_node, control, "value", this);
        auto& p = prop->addr;
        p.set_domain(control.domain());
        m_properties.push_back(std::move(prop));
      }
      catch(...)
      {
      }
    }
  }

  void add_outlet(Process::ControlOutlet& control)
  {
    if(!control.exposed().isEmpty())
    {
      try
      {
        m_properties.push_back(
            add_property<Process::Outlet::p_address>(
                this->node(), control, control.exposed().toStdString(), this));
        auto& port_node = m_properties.back()->addr.get_node();
        auto prop = add_value_property<Process::ControlOutlet::p_value>(
            port_node, control, "value", this);
        auto& p = prop->addr;
        p.set_domain(control.domain());
        m_properties.push_back(std::move(prop));
      }
      catch(...)
      {
      }
    }
  }
};

IntervalBase::IntervalBase(
    ossia::net::node_base& parent, Scenario::IntervalModel& interval,
    const score::DocumentContext& doc, QObject* parent_comp)
    : parent_t{parent, interval.metadata(), interval,
               doc,    "IntervalComponent", parent_comp}
    , m_processesNode{*node().create_child("processes")}
{
  using namespace Scenario;

  add_get<IntervalDurations::p_min>(interval.duration);
  add_get<IntervalDurations::p_max>(interval.duration);
  add_get<IntervalDurations::p_default>(interval.duration);
  add_get<IntervalDurations::p_percentage>(interval.duration);

  add<IntervalDurations::p_speed>(interval.duration);
  add<IntervalModel::p_muted>(interval);
}

ProcessComponent*
IntervalBase::make(ProcessComponentFactory& factory, Process::ProcessModel& process)
{
  return factory.make(m_processesNode, process, system(), this);
}

ProcessComponent* IntervalBase::make(Process::ProcessModel& process)
{
  if(!process.metadata().getName().isEmpty())
  {
    return new DefaultProcessComponent{m_processesNode, process, system(), this};
  }
  return nullptr;
}

bool IntervalBase::removing(
    const Process::ProcessModel& cst, const ProcessComponent& comp)
{
  return true;
}

Interval::~Interval() { }

}
