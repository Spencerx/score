#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Nodal/CommandFactory.hpp>
#include <Nodal/Commands.hpp>
#include <Nodal/Presenter.hpp>
#include <Nodal/Process.hpp>
#include <Nodal/View.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <QTimer>

namespace Nodal
{

Presenter::Presenter(
    const Model& layer, View* view, const Process::Context& ctx, QObject* parent)
    : Process::LayerPresenter{layer, view, ctx, parent}
    , m_model{layer}
    , m_view{view}
{
  bind(layer.nodes, *this);

  connect(view, &View::dropReceived, this, &Presenter::on_drop);

  if(auto itv = Scenario::closestParentInterval(m_model.parent()))
  {
    auto& dur = itv->duration;
    con(ctx.execTimer, &QTimer::timeout, this, [&] {
      {
        float p = ossia::max((float)dur.playPercentage(), 0.f);
        for(Process::NodeItem& node : m_nodes)
        {
          node.setPlayPercentage(p, dur.defaultDuration());
        }
      }
    });

    auto reset_exec = [this] {
      for(Process::NodeItem& node : m_nodes)
      {
        node.setPlayPercentage(0.f, TimeVal{});
      }
    };
    connect(&m_model, &Model::stopExecution, this, reset_exec);
    connect(&m_model, &Model::resetExecution, this, reset_exec);
  }
}

Presenter::~Presenter()
{
  m_nodes.remove_all();
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
  m_defaultW = defaultWidth;

  const auto d = m_model.duration();
  for(Process::NodeItem& node : m_nodes)
  {
    node.setParentDuration(d);
  }
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setOpacity(1);
}

void Presenter::putBehind()
{
  m_view->setOpacity(0.2);
}

void Presenter::on_zoomRatioChanged(ZoomRatio ratio)
{
  m_ratio = ratio;
  const auto d = m_model.duration();
  for(Process::NodeItem& node : m_nodes)
  {
    node.setParentDuration(d);
  }
}

void Presenter::parentGeometryChanged() { }

void Presenter::on_drop(const QPointF& pos, const QMimeData& mime)
{
  const auto& ctx = context().context;
  auto& layer = m_model;
  if(mime.hasFormat(score::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{mime};
    Process::ProcessData p = des.deserialize();

    auto cmd = new CreateNode(layer, pos, p.key, p.customData, getStrongId(layer.nodes));
    CommandDispatcher<> d{ctx.commandStack};
    d.submit(cmd);
  }
  else
  {
    // TODO refactor with EffectProcessLayer
    const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

    if(auto res = handlers.getDrop(mime, ctx); !res.empty())
    {
      RedoMacroCommandDispatcher<DropNodesMacro> cmd{ctx.commandStack};
      score::Dispatcher_T disp{cmd};
      for(const auto& proc : res)
      {
        auto& p = proc.creation;
        // TODO fudge pos a bit
        auto create
            = new CreateNode(layer, pos, p.key, p.customData, getStrongId(layer.nodes));
        cmd.submit(create);
        if(auto fx = layer.nodes.find(create->nodeId()); fx != layer.nodes.end())
        {
          if(proc.setup)
          {
            proc.setup(*fx, disp);
          }
        }
      }

      cmd.commit();
    }
  }
}

void Presenter::on_created(Process::ProcessModel& n)
{
  auto item = new Process::NodeItem{n, m_context.context, m_model.duration(), m_view};
  item->setPos(n.position());
  m_nodes.insert(item);
}

void Presenter::on_removing(const Process::ProcessModel& n)
{
  m_nodes.erase(n.id());
}
}
