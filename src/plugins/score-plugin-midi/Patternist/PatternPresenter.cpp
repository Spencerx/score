#include "PatternPresenter.hpp"

#include <Process/Focus/FocusDispatcher.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Patternist/Commands/PatternProperties.hpp>
#include <Patternist/PatternParsing.hpp>
#include <Patternist/PatternView.hpp>

namespace Patternist
{

Presenter::Presenter(
    const Patternist::ProcessModel& layer, View* view, const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{layer, view, ctx, parent}
    , m_view{view}
{
  putToFront();

  connect(m_view, &View::dropReceived, this, &Presenter::on_drop);

  connect(m_view, &View::toggled, this, [&](int lane, int index) {
    auto cur = layer.patterns()[layer.currentPattern()];
    const auto& l = cur.lanes[lane];
    auto b = l.pattern[index];

    switch(b)
    {
      case Note::Rest:
        cur.lanes[lane].pattern[index] = Note::Note;
        break;
      case Note::Note:
        cur.lanes[lane].pattern[index] = l.note < 128 ? Note::Legato : Note::Rest;
        break;
      case Note::Legato:
        cur.lanes[lane].pattern[index] = Note::Rest;
        break;
    }

    CommandDispatcher<> disp{m_context.context.commandStack};
    disp.submit<UpdatePattern>(layer, layer.currentPattern(), cur);
  });

  connect(m_view, &View::noteChanged, this, [&](int lane, int note) {
    auto cur = layer.patterns()[layer.currentPattern()];
    cur.lanes[lane].note = note;

    auto& disp = m_context.context.dispatcher;
    disp.submit<UpdatePattern>(layer, layer.currentPattern(), cur);
  });

  connect(m_view, &View::noteChangeFinished, this, [&] {
    auto& disp = m_context.context.dispatcher;
    disp.commit();
  });
}

Presenter::~Presenter() { }
void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setEnabled(true);
}

void Presenter::putBehind()
{
  m_view->setEnabled(false);
}

void Presenter::on_zoomRatioChanged(ZoomRatio zr) { }

void Presenter::parentGeometryChanged() { }

void Presenter::on_drop(const QPointF& pos, const QMimeData& md)
{
  auto patterns = parsePatternFiles(md);
  if(patterns.empty())
    return;
  if(patterns[0].empty())
    return;

  auto& model = static_cast<const Patternist::ProcessModel&>(this->model());
  CommandDispatcher<> disp{m_context.context.commandStack};
  disp.submit<UpdatePattern>(model, model.currentPattern(), patterns[0][0]);
}
}
