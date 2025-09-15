#include "Executor.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Vst3/Control.hpp>
#include <Vst3/Node.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/domain/domain.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(vst3::Executor)
namespace vst3
{

namespace
{
// This could be a lambda but fails on GCC 8 apparently because
// it tries to reach QObject::connect through the this-> pointer ?
template <typename Node_T>
struct ConnectValueChanged
{
  Executor* self{};
  std::size_t queue_idx{};
  Node_T n;
  QPointer<vst3::ControlInlet> ctrl;

  void operator()() const noexcept
  {
    QObject::connect(
        ctrl, &vst3::ControlInlet::valueChanged, self,
        [queue_idx = this->queue_idx, n = this->n](const ossia::value& v) {
      n->set_control(queue_idx, ossia::convert<float>(v));
    });
  }
};
}
template <typename Node_T>
void Executor::setupNode(Node_T& node)
{
  const auto& proc = this->process();
  node->controls.reserve(proc.controls.size());
  const auto& inlets = proc.inlets();

  for(auto model_inlet : inlets)
  {
    if(auto ctrl = qobject_cast<vst3::ControlInlet*>(model_inlet))
    {
      auto queue_idx
          = node->add_control(new ossia::value_inlet, ctrl->fxNum, ctrl->value());
      connect(
          ctrl, &vst3::ControlInlet::valueChanged, this,
          [queue_idx, node](const ossia::value& v) {
        node->set_control(queue_idx, ossia::convert<float>(v));
      });
    }
  }

  std::weak_ptr<std::remove_reference_t<decltype(*node)>> wp = node;
  connect(
      &proc, &vst3::Model::controlAdded, this,
      [this, &proc, wp](const Id<Process::Port>& id) {
    QPointer<vst3::ControlInlet> ctrl = proc.getControl(id);
    if(!ctrl)
      return;
    if(const Node_T& n = wp.lock())
    {
      Execution::SetupContext& setup = system().context().setup;
      auto inlet = new ossia::value_inlet;

      Execution::Transaction commands{system()};

      commands.push_back([n, inlet, ctrl, val = ctrl->value(), num = ctrl->fxNum,
                          self = this, qed_ptr = weak_edit] {
        auto qed = qed_ptr.lock();
        if(!qed)
          return;
        auto queue_idx = n->add_control(inlet, num, val);
        qed->enqueue(ConnectValueChanged<Node_T>{self, queue_idx, n, ctrl});
      });

      setup.register_inlet(*ctrl, inlet, n, commands);

      commands.run_all();
    }
      });
  connect(
      &proc, &vst3::Model::controlRemoved, this, [this, wp](const Process::Port& port) {
        if(auto n = wp.lock())
        {
          Execution::SetupContext& setup = system().context().setup;
          in_exec([n, num = static_cast<const vst3::ControlInlet&>(port).fxNum] {
            auto it = ossia::find_if(n->controls, [&](auto& c) { return c.idx == num; });
            if(it != n->controls.end())
            {
              ossia::value_port* port = it->port;
              n->controls.erase(it);
              auto port_it = ossia::find_if(n->root_inputs(), [&](auto& p) {
                return p->template target<ossia::value_port>() == port;
              });
              if(port_it != n->root_inputs().end())
              {
                port->clear();
                n->root_inputs().erase(port_it);
              }
            }
          });
          setup.unregister_inlet(static_cast<const Process::Inlet&>(port), n);
        }
      });
}

Executor::Executor(vst3::Model& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "VST3Component", parent}
{
  if(!proc.fx)
    throw std::runtime_error("Unable to load VST");

  if(proc.fx.supports_double)
  {
    auto n = vst3::make_vst_fx<true>(*ctx.execState, proc.fx, ctx.execState->sampleRate);
    setupNode(n);
    node = std::move(n);
  }
  else
  {
    auto n
        = vst3::make_vst_fx<false>(*ctx.execState, proc.fx, ctx.execState->sampleRate);
    setupNode(n);
    node = std::move(n);
  }
  m_ossia_process = std::make_shared<ossia::node_process>(node);
}

}
