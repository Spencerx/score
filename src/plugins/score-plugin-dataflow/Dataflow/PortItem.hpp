#pragma once
#include <Process/Dataflow/AddressAccessorEditWidget.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/plugins/SerializableHelpers.hpp>

#include <score_plugin_dataflow_export.h>
namespace Scenario
{
class IntervalModel;
}
namespace Dataflow
{
class CableItem;

class SCORE_PLUGIN_DATAFLOW_EXPORT AutomatablePortItem : public PortItem
{
public:
  using PortItem::PortItem;
  ~AutomatablePortItem() override;

  void setupMenu(QMenu&, const score::DocumentContext& ctx) override;
  void on_createAutomation(const score::DocumentContext& m_context);
  virtual bool on_createAutomation(
      const Scenario::IntervalModel& parent, std::function<void(score::Command*)> macro,
      const score::DocumentContext& m_context);

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;
};

class SCORE_PLUGIN_DATAFLOW_EXPORT AutomatablePortFactory : public Process::PortFactory
{
public:
  ~AutomatablePortFactory() override = default;

private:
  Dataflow::PortItem* makePortItem(
      Process::Inlet& port, const Process::Context& ctx, QGraphicsItem* parent,
      QObject* context) override;

  Dataflow::PortItem* makePortItem(
      Process::Outlet& port, const Process::Context& ctx, QGraphicsItem* parent,
      QObject* context) override;
};

template <typename Model_T>
class AutomatablePortFactory_T final : public AutomatablePortFactory
{
public:
  ~AutomatablePortFactory_T() override = default;

private:
  UuidKey<Process::Port> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  Model_T* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }
};

SCORE_PLUGIN_DATAFLOW_EXPORT
void replaceCable(
    const score::DocumentContext& ctx, const Process::Cable& currentCable,
    const Process::Port& newPort);
}
