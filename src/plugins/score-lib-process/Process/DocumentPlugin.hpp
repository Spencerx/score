#pragma once
#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <ossia/detail/ptr_set.hpp>

#include <verdigris>
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT DataflowManager final : public QObject
{
  W_OBJECT(DataflowManager)
public:
  DataflowManager();
  ~DataflowManager();

  using cable_map = ossia::ptr_map<const Process::Cable*, Dataflow::CableItem*>;
  using port_map = ossia::ptr_map<const Process::Port*, Dataflow::PortItem*>;

  Dataflow::CableItem* createCable(
      const Process::Cable& cable, const Process::Context& context,
      QGraphicsScene* scene);

  cable_map& cables() noexcept { return m_cableMap; }
  port_map& ports() noexcept { return m_portMap; }

private:
  cable_map m_cableMap;
  port_map m_portMap;
};
}
