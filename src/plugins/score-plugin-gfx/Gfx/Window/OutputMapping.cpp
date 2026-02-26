#include "OutputMapping.hpp"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>

namespace Gfx
{

// --- OutputMappingItem implementation ---

OutputMappingItem::OutputMappingItem(
    int index, const QRectF& rect, QGraphicsItem* parent)
    : QGraphicsRectItem(rect, parent)
    , m_index{index}
{
  setFlags(
      QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable
      | QGraphicsItem::ItemSendsGeometryChanges);
  setAcceptHoverEvents(true);
  setPen(QPen(Qt::white, 1));
  setBrush(QBrush(QColor(100, 150, 255, 80)));
}

void OutputMappingItem::setOutputIndex(int idx)
{
  m_index = idx;
  update();
}

void OutputMappingItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  QGraphicsRectItem::paint(painter, option, widget);

  // Draw index label
  painter->setPen(Qt::white);
  auto font = painter->font();
  font.setPixelSize(14);
  painter->setFont(font);
  painter->drawText(rect(), Qt::AlignCenter, QString::number(m_index));

  // Draw blend zones as gradient overlays
  auto r = rect();
  auto drawBlendZone = [&](const EdgeBlend& blend, const QRectF& zone, bool horizontal) {
    if(blend.width <= 0.0f)
      return;
    QLinearGradient grad;
    QColor blendColor(255, 200, 50, 100);
    QColor transparent(255, 200, 50, 0);
    if(horizontal)
    {
      grad.setStart(zone.left(), zone.center().y());
      grad.setFinalStop(zone.right(), zone.center().y());
    }
    else
    {
      grad.setStart(zone.center().x(), zone.top());
      grad.setFinalStop(zone.center().x(), zone.bottom());
    }
    grad.setColorAt(0, blendColor);
    grad.setColorAt(1, transparent);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(grad));
    painter->drawRect(zone);
  };

  // Left blend: gradient from left edge inward
  if(blendLeft.width > 0.0f)
  {
    double w = blendLeft.width * r.width();
    drawBlendZone(blendLeft, QRectF(r.left(), r.top(), w, r.height()), true);
  }
  // Right blend: gradient from right edge inward
  if(blendRight.width > 0.0f)
  {
    double w = blendRight.width * r.width();
    QLinearGradient grad(r.right(), r.center().y(), r.right() - w, r.center().y());
    grad.setColorAt(0, QColor(255, 200, 50, 100));
    grad.setColorAt(1, QColor(255, 200, 50, 0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(grad));
    painter->drawRect(QRectF(r.right() - w, r.top(), w, r.height()));
  }
  // Top blend: gradient from top edge inward
  if(blendTop.width > 0.0f)
  {
    double h = blendTop.width * r.height();
    drawBlendZone(blendTop, QRectF(r.left(), r.top(), r.width(), h), false);
  }
  // Bottom blend: gradient from bottom edge inward
  if(blendBottom.width > 0.0f)
  {
    double h = blendBottom.width * r.height();
    QLinearGradient grad(r.center().x(), r.bottom(), r.center().x(), r.bottom() - h);
    grad.setColorAt(0, QColor(255, 200, 50, 100));
    grad.setColorAt(1, QColor(255, 200, 50, 0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(grad));
    painter->drawRect(QRectF(r.left(), r.bottom() - h, r.width(), h));
  }

  // Draw blend handle lines at the boundary of each blend zone.
  // Always draw a subtle dotted line at the handle position (even when width=0)
  // so the user knows the handle is grabbable; draw a stronger line when active.
  {
    constexpr double minInset = 10.0;
    painter->setBrush(Qt::NoBrush);

    auto drawHandle = [&](float blendWidth, bool horizontal, bool fromStart) {
      double offset;
      if(horizontal)
        offset = std::max((double)blendWidth * r.width(), minInset);
      else
        offset = std::max((double)blendWidth * r.height(), minInset);

      // Active blend: bright dashed line; inactive: dim dotted line
      if(blendWidth > 0.0f)
        painter->setPen(QPen(QColor(255, 200, 50, 200), 1.5, Qt::DashLine));
      else
        painter->setPen(QPen(QColor(255, 200, 50, 60), 1, Qt::DotLine));

      if(horizontal)
      {
        double x = fromStart ? r.left() + offset : r.right() - offset;
        painter->drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
      }
      else
      {
        double y = fromStart ? r.top() + offset : r.bottom() - offset;
        painter->drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
      }
    };

    drawHandle(blendLeft.width, true, true);
    drawHandle(blendRight.width, true, false);
    drawHandle(blendTop.width, false, true);
    drawHandle(blendBottom.width, false, false);
  }

  // Draw selection highlight
  if(isSelected())
  {
    QPen selPen(Qt::yellow, 2, Qt::DashLine);
    painter->setPen(selPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect());
  }
}

int OutputMappingItem::hitTestEdges(const QPointF& pos) const
{
  constexpr double margin = 6.0;
  int edges = None;
  auto r = rect();
  if(pos.x() - r.left() < margin)
    edges |= Left;
  if(r.right() - pos.x() < margin)
    edges |= Right;
  if(pos.y() - r.top() < margin)
    edges |= Top;
  if(r.bottom() - pos.y() < margin)
    edges |= Bottom;
  return edges;
}

OutputMappingItem::BlendHandle
OutputMappingItem::hitTestBlendHandles(const QPointF& pos) const
{
  constexpr double handleMargin = 5.0;
  // Minimum inset from the edge so the handle zone never overlaps with the
  // 6px resize margin. This ensures handles are grabbable even at width=0.
  constexpr double minInset = 10.0;
  auto r = rect();

  {
    double handleX = r.left() + std::max((double)blendLeft.width * r.width(), minInset);
    if(std::abs(pos.x() - handleX) < handleMargin && pos.y() >= r.top()
       && pos.y() <= r.bottom())
      return BlendLeft;
  }
  {
    double handleX
        = r.right() - std::max((double)blendRight.width * r.width(), minInset);
    if(std::abs(pos.x() - handleX) < handleMargin && pos.y() >= r.top()
       && pos.y() <= r.bottom())
      return BlendRight;
  }
  {
    double handleY = r.top() + std::max((double)blendTop.width * r.height(), minInset);
    if(std::abs(pos.y() - handleY) < handleMargin && pos.x() >= r.left()
       && pos.x() <= r.right())
      return BlendTop;
  }
  {
    double handleY
        = r.bottom() - std::max((double)blendBottom.width * r.height(), minInset);
    if(std::abs(pos.y() - handleY) < handleMargin && pos.x() >= r.left()
       && pos.x() <= r.right())
      return BlendBottom;
  }
  return BlendNone;
}

QVariant OutputMappingItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
  if(change == ItemPositionChange && scene())
  {
    // Visual scene rect = pos + rect, so clamp pos such that
    // pos + rect.topLeft >= sceneRect.topLeft and pos + rect.bottomRight <= sceneRect.bottomRight
    QPointF newPos = value.toPointF();
    auto r = rect();
    auto sr = scene()->sceneRect();

    double minX = sr.left() - r.left();
    double maxX = sr.right() - r.right();
    double minY = sr.top() - r.top();
    double maxY = sr.bottom() - r.bottom();
    newPos.setX(qBound(std::min(minX, maxX), newPos.x(), std::max(minX, maxX)));
    newPos.setY(qBound(std::min(minY, maxY), newPos.y(), std::max(minY, maxY)));
    return newPos;
  }
  if(change == ItemPositionHasChanged)
  {
    if(onChanged)
      onChanged();
  }
  return QGraphicsRectItem::itemChange(change, value);
}

void OutputMappingItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  // Check blend handles first (they are inside the rect, so test before edges)
  m_blendHandle = hitTestBlendHandles(event->pos());
  if(m_blendHandle != BlendNone)
  {
    m_dragStart = event->pos();
    event->accept();
    return;
  }

  m_resizeEdges = hitTestEdges(event->pos());
  if(m_resizeEdges != None)
  {
    m_dragStart = event->scenePos();
    m_rectStart = rect();
    event->accept();
  }
  else
  {
    // Store anchor for precision move (Ctrl)
    m_moveAnchorScene = event->scenePos();
    m_posAtPress = pos();
    QGraphicsRectItem::mousePressEvent(event);
  }
}

void OutputMappingItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  const double scale = (event->modifiers() & Qt::ControlModifier) ? 0.1 : 1.0;

  if(m_blendHandle != BlendNone)
  {
    auto r = rect();
    // Apply precision: scale the delta from drag start
    auto rawPos = event->pos();
    auto pos = m_dragStart + (rawPos - m_dragStart) * scale;

    switch(m_blendHandle)
    {
      case BlendLeft: {
        double newW = qBound(0.0, (pos.x() - r.left()) / r.width(), 0.5);
        blendLeft.width = (float)newW;
        break;
      }
      case BlendRight: {
        double newW = qBound(0.0, (r.right() - pos.x()) / r.width(), 0.5);
        blendRight.width = (float)newW;
        break;
      }
      case BlendTop: {
        double newH = qBound(0.0, (pos.y() - r.top()) / r.height(), 0.5);
        blendTop.width = (float)newH;
        break;
      }
      case BlendBottom: {
        double newH = qBound(0.0, (r.bottom() - pos.y()) / r.height(), 0.5);
        blendBottom.width = (float)newH;
        break;
      }
      default:
        break;
    }

    update();
    if(onChanged)
      onChanged();
    event->accept();
    return;
  }

  if(m_resizeEdges != None)
  {
    auto rawDelta = event->scenePos() - m_dragStart;
    auto delta = rawDelta * scale;
    QRectF r = m_rectStart;
    constexpr double minSize = 10.0;

    // Scene bounds in item-local coordinates (account for pos() offset)
    auto sr = scene()->sceneRect();
    auto p = pos();
    double sLeft = sr.left() - p.x();
    double sRight = sr.right() - p.x();
    double sTop = sr.top() - p.y();
    double sBottom = sr.bottom() - p.y();

    if(m_resizeEdges & Left)
    {
      double newLeft = qMin(r.left() + delta.x(), r.right() - minSize);
      r.setLeft(qMax(newLeft, sLeft));
    }
    if(m_resizeEdges & Right)
    {
      double newRight = qMax(r.right() + delta.x(), r.left() + minSize);
      r.setRight(qMin(newRight, sRight));
    }
    if(m_resizeEdges & Top)
    {
      double newTop = qMin(r.top() + delta.y(), r.bottom() - minSize);
      r.setTop(qMax(newTop, sTop));
    }
    if(m_resizeEdges & Bottom)
    {
      double newBottom = qMax(r.bottom() + delta.y(), r.top() + minSize);
      r.setBottom(qMin(newBottom, sBottom));
    }

    setRect(r);
    if(onChanged)
      onChanged();
    event->accept();
  }
  else
  {
    // Precision move: bypass default handler, compute position manually
    auto rawDelta = event->scenePos() - m_moveAnchorScene;
    auto delta = rawDelta * scale;
    setPos(m_posAtPress + delta);
  }
}

void OutputMappingItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_blendHandle = BlendNone;
  m_resizeEdges = None;
  QGraphicsRectItem::mouseReleaseEvent(event);
}

void OutputMappingItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  // Check blend handles first
  auto bh = hitTestBlendHandles(event->pos());
  if(bh == BlendLeft || bh == BlendRight)
  {
    setCursor(Qt::SplitHCursor);
    QGraphicsRectItem::hoverMoveEvent(event);
    return;
  }
  if(bh == BlendTop || bh == BlendBottom)
  {
    setCursor(Qt::SplitVCursor);
    QGraphicsRectItem::hoverMoveEvent(event);
    return;
  }

  int edges = hitTestEdges(event->pos());
  if((edges & Left) && (edges & Top))
    setCursor(Qt::SizeFDiagCursor);
  else if((edges & Right) && (edges & Bottom))
    setCursor(Qt::SizeFDiagCursor);
  else if((edges & Left) && (edges & Bottom))
    setCursor(Qt::SizeBDiagCursor);
  else if((edges & Right) && (edges & Top))
    setCursor(Qt::SizeBDiagCursor);
  else if(edges & (Left | Right))
    setCursor(Qt::SizeHorCursor);
  else if(edges & (Top | Bottom))
    setCursor(Qt::SizeVerCursor);
  else
    setCursor(Qt::ArrowCursor);

  QGraphicsRectItem::hoverMoveEvent(event);
}

// --- CornerWarpCanvas implementation ---

CornerWarpCanvas::CornerWarpCanvas(QWidget* parent)
    : QGraphicsView(parent)
{
  setScene(&m_scene);
  setFixedSize(210, 210);
  m_scene.setSceneRect(0, 0, m_canvasSize, m_canvasSize);
  setRenderHint(QPainter::Antialiasing);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  // Border representing the unwarped output window (at 75% of canvas, centered)
  const double margin = m_canvasSize * 0.125;
  const double inner = m_canvasSize * 0.75;
  m_border = m_scene.addRect(margin, margin, inner, inner, QPen(Qt::darkGray, 1));

  // Quad outline
  m_quadOutline = m_scene.addPolygon(QPolygonF(), QPen(QColor(100, 200, 255), 2));

  // Corner handles: TL, TR, BL, BR
  const double handleR = 6.0;
  const QColor handleColors[4]
      = {QColor(255, 80, 80), QColor(80, 255, 80), QColor(80, 80, 255),
         QColor(255, 255, 80)};

  for(int i = 0; i < 4; i++)
  {
    m_handles[i] = m_scene.addEllipse(
        -handleR, -handleR, handleR * 2, handleR * 2, QPen(Qt::white, 1),
        QBrush(handleColors[i]));
    m_handles[i]->setFlag(QGraphicsItem::ItemIsMovable, true);
    m_handles[i]->setZValue(10);
  }

  rebuildItems();
}

void CornerWarpCanvas::setWarp(const CornerWarp& warp)
{
  m_warp = warp;
  rebuildItems();
}

CornerWarp CornerWarpCanvas::getWarp() const
{
  return m_warp;
}

void CornerWarpCanvas::resetWarp()
{
  m_warp = CornerWarp{};
  rebuildItems();
  if(onChanged)
    onChanged();
}

void CornerWarpCanvas::setEnabled(bool enabled)
{
  for(int i = 0; i < 4; i++)
    m_handles[i]->setFlag(QGraphicsItem::ItemIsMovable, enabled);
  QGraphicsView::setEnabled(enabled);
}

void CornerWarpCanvas::resizeEvent(QResizeEvent* event)
{
  QGraphicsView::resizeEvent(event);
  fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
}

void CornerWarpCanvas::rebuildItems()
{
  const double s = m_canvasSize;
  const double margin = s * 0.125;
  const double inner = s * 0.75;

  // Map UV [0,1] to the central 75% of the canvas (with 12.5% margin on each side)
  auto toScene = [margin, inner](const QPointF& uv) -> QPointF {
    return QPointF(margin + uv.x() * inner, margin + uv.y() * inner);
  };

  const QPointF corners[4]
      = {toScene(m_warp.topLeft), toScene(m_warp.topRight), toScene(m_warp.bottomLeft),
         toScene(m_warp.bottomRight)};

  // Position handles (blocking scene change events during setup)
  for(int i = 0; i < 4; i++)
  {
    m_handles[i]->setPos(corners[i]);
  }

  updateLinesAndGrid();
}

void CornerWarpCanvas::updateLinesAndGrid()
{
  const double s = m_canvasSize;

  // Read current positions from handles
  QPointF tl = m_handles[0]->pos();
  QPointF tr = m_handles[1]->pos();
  QPointF bl = m_handles[2]->pos();
  QPointF br = m_handles[3]->pos();

  // Update quad outline
  QPolygonF quad;
  quad << tl << tr << br << bl << tl;
  m_quadOutline->setPolygon(quad);

  // Update grid lines (4x4 bilinear subdivision)
  for(auto* line : m_gridLines)
  {
    m_scene.removeItem(line);
    delete line;
  }
  m_gridLines.clear();

  constexpr int gridN = 4;
  QPen gridPen(QColor(100, 200, 255, 80), 1);

  auto bilinear = [&](double u, double v) -> QPointF {
    return (1 - u) * (1 - v) * tl + u * (1 - v) * tr + (1 - u) * v * bl + u * v * br;
  };

  // Horizontal grid lines
  for(int r = 1; r < gridN; r++)
  {
    double v = (double)r / gridN;
    for(int c = 0; c < gridN; c++)
    {
      double u0 = (double)c / gridN;
      double u1 = (double)(c + 1) / gridN;
      auto* line = m_scene.addLine(QLineF(bilinear(u0, v), bilinear(u1, v)), gridPen);
      m_gridLines.push_back(line);
    }
  }

  // Vertical grid lines
  for(int c = 1; c < gridN; c++)
  {
    double u = (double)c / gridN;
    for(int r = 0; r < gridN; r++)
    {
      double v0 = (double)r / gridN;
      double v1 = (double)(r + 1) / gridN;
      auto* line = m_scene.addLine(QLineF(bilinear(u, v0), bilinear(u, v1)), gridPen);
      m_gridLines.push_back(line);
    }
  }
}

// We override the scene's mouse events via event filter style by installing
// on the viewport. Instead, use the simpler approach: check handle positions
// on scene changes.
// Actually, let's use a simpler approach: override mousePressEvent/mouseMoveEvent/mouseReleaseEvent
// to track when handles move.

void CornerWarpCanvas::mousePressEvent(QMouseEvent* event)
{
  // Check if we're pressing on a handle
  auto scenePos = mapToScene(event->pos());
  m_dragHandle = -1;
  for(int i = 0; i < 4; i++)
  {
    if(QLineF(scenePos, m_handles[i]->pos()).length() < 10.0)
    {
      m_dragHandle = i;
      m_dragging = true;
      m_handleAnchor = m_handles[i]->pos();
      m_mouseAnchor = scenePos;
      break;
    }
  }

  if(m_dragHandle < 0)
    QGraphicsView::mousePressEvent(event);
}

void CornerWarpCanvas::mouseMoveEvent(QMouseEvent* event)
{
  if(m_dragging && m_dragHandle >= 0)
  {
    const double scale = (event->modifiers() & Qt::ControlModifier) ? 0.1 : 1.0;
    const double s = m_canvasSize;
    const double margin = s * 0.125;
    const double inner = s * 0.75;

    auto scenePos = mapToScene(event->pos());
    auto rawDelta = scenePos - m_mouseAnchor;
    auto targetPos = m_handleAnchor + rawDelta * scale;

    // Clamp handle to canvas bounds so it can never be lost
    double cx = qBound(0.0, targetPos.x(), s);
    double cy = qBound(0.0, targetPos.y(), s);
    m_handles[m_dragHandle]->setPos(cx, cy);
    // Convert scene coords back to UV
    double u = (cx - margin) / inner;
    double v = (cy - margin) / inner;

    QPointF* corners[4]
        = {&m_warp.topLeft, &m_warp.topRight, &m_warp.bottomLeft, &m_warp.bottomRight};
    *corners[m_dragHandle] = QPointF(u, v);

    updateLinesAndGrid();

    if(onChanged)
      onChanged();
  }
  else
  {
    QGraphicsView::mouseMoveEvent(event);
  }
}

void CornerWarpCanvas::mouseReleaseEvent(QMouseEvent* event)
{
  QGraphicsView::mouseReleaseEvent(event);
  m_dragging = false;
  m_dragHandle = -1;
}

// --- OutputMappingCanvas implementation ---

OutputMappingCanvas::OutputMappingCanvas(QWidget* parent)
    : QGraphicsView(parent)
{
  setScene(&m_scene);
  m_scene.setSceneRect(0, 0, m_canvasWidth, m_canvasHeight);
  setMinimumSize(200, 150);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setBackgroundBrush(QBrush(QColor(40, 40, 40)));
  setRenderHint(QPainter::Antialiasing);

  // Draw border around the full texture area
  m_border = m_scene.addRect(0, 0, m_canvasWidth, m_canvasHeight, QPen(Qt::gray, 1));
  m_border->setZValue(-1);

  connect(&m_scene, &QGraphicsScene::selectionChanged, this, [this] {
    if(onSelectionChanged)
    {
      auto items = m_scene.selectedItems();
      if(!items.isEmpty())
      {
        if(auto* item = dynamic_cast<OutputMappingItem*>(items.first()))
          onSelectionChanged(item->outputIndex());
      }
      else
      {
        onSelectionChanged(-1);
      }
    }
  });
}

OutputMappingCanvas::~OutputMappingCanvas()
{
  // Clear callbacks before the scene destroys items (which triggers selectionChanged)
  onSelectionChanged = nullptr;
  onItemGeometryChanged = nullptr;
}

void OutputMappingCanvas::resizeEvent(QResizeEvent* event)
{
  QGraphicsView::resizeEvent(event);
  fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
}

void OutputMappingCanvas::updateAspectRatio(int inputWidth, int inputHeight)
{
  if(inputWidth < 1 || inputHeight < 1)
    return;

  // Keep the longer axis at 400px, scale the other to match the aspect ratio
  constexpr double maxDim = 400.0;
  double aspect = (double)inputWidth / (double)inputHeight;
  double oldW = m_canvasWidth;
  double oldH = m_canvasHeight;

  if(aspect >= 1.0)
  {
    m_canvasWidth = maxDim;
    m_canvasHeight = maxDim / aspect;
  }
  else
  {
    m_canvasHeight = maxDim;
    m_canvasWidth = maxDim * aspect;
  }

  // Rescale existing mapping items from old dimensions to new
  if(oldW > 0 && oldH > 0)
  {
    double sx = m_canvasWidth / oldW;
    double sy = m_canvasHeight / oldH;

    for(auto* item : m_scene.items())
    {
      if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
      {
        auto r = mi->mapRectToScene(mi->rect());
        QRectF scaled(r.x() * sx, r.y() * sy, r.width() * sx, r.height() * sy);
        mi->setPos(0, 0);
        mi->setRect(scaled);
      }
    }
  }

  m_scene.setSceneRect(0, 0, m_canvasWidth, m_canvasHeight);
  if(m_border)
    m_border->setRect(0, 0, m_canvasWidth, m_canvasHeight);
  fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
}

void OutputMappingCanvas::setupItemCallbacks(OutputMappingItem* item)
{
  item->onChanged = [this, item] {
    if(onItemGeometryChanged)
      onItemGeometryChanged(item->outputIndex());
  };
}

void OutputMappingCanvas::setMappings(const std::vector<OutputMapping>& mappings)
{
  // Remove existing mapping items
  QList<QGraphicsItem*> toRemove;
  for(auto* item : m_scene.items())
  {
    if(dynamic_cast<OutputMappingItem*>(item))
      toRemove.append(item);
  }
  for(auto* item : toRemove)
  {
    m_scene.removeItem(item);
    delete item;
  }

  for(int i = 0; i < (int)mappings.size(); ++i)
  {
    const auto& m = mappings[i];
    QRectF sceneRect(
        m.sourceRect.x() * canvasWidth(), m.sourceRect.y() * canvasHeight(),
        m.sourceRect.width() * canvasWidth(), m.sourceRect.height() * canvasHeight());
    auto* item = new OutputMappingItem(i, sceneRect);
    item->screenIndex = m.screenIndex;
    item->windowPosition = m.windowPosition;
    item->windowSize = m.windowSize;
    item->fullscreen = m.fullscreen;
    item->blendLeft = m.blendLeft;
    item->blendRight = m.blendRight;
    item->blendTop = m.blendTop;
    item->blendBottom = m.blendBottom;
    item->cornerWarp = m.cornerWarp;
    setupItemCallbacks(item);
    m_scene.addItem(item);
  }
}

std::vector<OutputMapping> OutputMappingCanvas::getMappings() const
{
  std::vector<OutputMapping> result;
  QList<OutputMappingItem*> items;

  for(auto* item : m_scene.items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
      items.append(mi);
  }

  // Sort by index
  std::sort(items.begin(), items.end(), [](auto* a, auto* b) {
    return a->outputIndex() < b->outputIndex();
  });

  for(auto* item : items)
  {
    OutputMapping m;
    auto r = item->mapRectToScene(item->rect());
    m.sourceRect = QRectF(
        r.x() / canvasWidth(), r.y() / canvasHeight(), r.width() / canvasWidth(),
        r.height() / canvasHeight());
    m.screenIndex = item->screenIndex;
    m.windowPosition = item->windowPosition;
    m.windowSize = item->windowSize;
    m.fullscreen = item->fullscreen;
    m.blendLeft = item->blendLeft;
    m.blendRight = item->blendRight;
    m.blendTop = item->blendTop;
    m.blendBottom = item->blendBottom;
    m.cornerWarp = item->cornerWarp;
    result.push_back(m);
  }
  return result;
}

void OutputMappingCanvas::addOutput()
{
  int count = 0;
  for(auto* item : m_scene.items())
    if(dynamic_cast<OutputMappingItem*>(item))
      count++;

  // Place new output at a default position
  double x = (count * 30) % (int)(canvasWidth() - 100);
  double y = (count * 30) % (int)(canvasHeight() - 75);
  auto* item = new OutputMappingItem(count, QRectF(x, y, 100, 75));
  setupItemCallbacks(item);
  m_scene.addItem(item);

  // Auto-select the new item so that property auto-match runs
  m_scene.clearSelection();
  item->setSelected(true);
}

void OutputMappingCanvas::removeSelectedOutput()
{
  auto items = m_scene.selectedItems();
  for(auto* item : items)
  {
    if(dynamic_cast<OutputMappingItem*>(item))
    {
      m_scene.removeItem(item);
      delete item;
    }
  }

  // Re-index remaining items
  QList<OutputMappingItem*> remaining;
  for(auto* item : m_scene.items())
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
      remaining.append(mi);

  std::sort(remaining.begin(), remaining.end(), [](auto* a, auto* b) {
    return a->outputIndex() < b->outputIndex();
  });

  for(int i = 0; i < remaining.size(); ++i)
    remaining[i]->setOutputIndex(i);
}
}

template <>
void JSONReader::read(const Gfx::OutputMapping& n)
{
  stream.StartObject();
  stream.Key("SourceRect");
  stream.StartArray();
  stream.Double(n.sourceRect.x());
  stream.Double(n.sourceRect.y());
  stream.Double(n.sourceRect.width());
  stream.Double(n.sourceRect.height());
  stream.EndArray();
  stream.Key("ScreenIndex");
  stream.Int(n.screenIndex);
  stream.Key("WindowPosition");
  stream.StartArray();
  stream.Int(n.windowPosition.x());
  stream.Int(n.windowPosition.y());
  stream.EndArray();
  stream.Key("WindowSize");
  stream.StartArray();
  stream.Int(n.windowSize.width());
  stream.Int(n.windowSize.height());
  stream.EndArray();
  stream.Key("Fullscreen");
  stream.Bool(n.fullscreen);

  auto writeBlend = [&](const char* key, const Gfx::EdgeBlend& b) {
    stream.Key(key);
    stream.StartArray();
    stream.Double(b.width);
    stream.Double(b.gamma);
    stream.EndArray();
  };
  writeBlend("BlendLeft", n.blendLeft);
  writeBlend("BlendRight", n.blendRight);
  writeBlend("BlendTop", n.blendTop);
  writeBlend("BlendBottom", n.blendBottom);

  if(!n.cornerWarp.isIdentity())
  {
    stream.Key("CornerWarp");
    stream.StartArray();
    stream.Double(n.cornerWarp.topLeft.x());
    stream.Double(n.cornerWarp.topLeft.y());
    stream.Double(n.cornerWarp.topRight.x());
    stream.Double(n.cornerWarp.topRight.y());
    stream.Double(n.cornerWarp.bottomLeft.x());
    stream.Double(n.cornerWarp.bottomLeft.y());
    stream.Double(n.cornerWarp.bottomRight.x());
    stream.Double(n.cornerWarp.bottomRight.y());
    stream.EndArray();
  }

  stream.EndObject();
}

template <>
void DataStreamReader::read(const Gfx::OutputMapping& n)
{
  m_stream << n.sourceRect << n.screenIndex << n.windowPosition << n.windowSize
           << n.fullscreen;
  m_stream << n.blendLeft.width << n.blendLeft.gamma;
  m_stream << n.blendRight.width << n.blendRight.gamma;
  m_stream << n.blendTop.width << n.blendTop.gamma;
  m_stream << n.blendBottom.width << n.blendBottom.gamma;
  m_stream << n.cornerWarp.topLeft << n.cornerWarp.topRight << n.cornerWarp.bottomLeft
           << n.cornerWarp.bottomRight;
}

template <>
void DataStreamWriter::write(Gfx::OutputMapping& n)
{
  m_stream >> n.sourceRect >> n.screenIndex >> n.windowPosition >> n.windowSize
      >> n.fullscreen;
  m_stream >> n.blendLeft.width >> n.blendLeft.gamma;
  m_stream >> n.blendRight.width >> n.blendRight.gamma;
  m_stream >> n.blendTop.width >> n.blendTop.gamma;
  m_stream >> n.blendBottom.width >> n.blendBottom.gamma;
  m_stream >> n.cornerWarp.topLeft >> n.cornerWarp.topRight >> n.cornerWarp.bottomLeft
      >> n.cornerWarp.bottomRight;
}
template <>
void JSONWriter::write(Gfx::OutputMapping& n)
{
  if(auto sr = obj.tryGet("SourceRect"))
  {
    auto arr = sr->toArray();
    if(arr.Size() == 4)
      n.sourceRect = QRectF(
          arr[0].GetDouble(), arr[1].GetDouble(), arr[2].GetDouble(),
          arr[3].GetDouble());
  }
  if(auto v = obj.tryGet("ScreenIndex"))
    n.screenIndex = v->toInt();
  if(auto wp = obj.tryGet("WindowPosition"))
  {
    auto arr = wp->toArray();
    if(arr.Size() == 2)
      n.windowPosition = QPoint(arr[0].GetInt(), arr[1].GetInt());
  }
  if(auto ws = obj.tryGet("WindowSize"))
  {
    auto arr = ws->toArray();
    if(arr.Size() == 2)
      n.windowSize = QSize(arr[0].GetInt(), arr[1].GetInt());
  }
  if(auto v = obj.tryGet("Fullscreen"))
    n.fullscreen = v->toBool();

  auto readBlend = [&](const std::string& key, Gfx::EdgeBlend& b) {
    if(auto v = obj.tryGet(key))
    {
      auto arr = v->toArray();
      if(arr.Size() == 2)
      {
        b.width = (float)arr[0].GetDouble();
        b.gamma = (float)arr[1].GetDouble();
      }
    }
  };
  readBlend("BlendLeft", n.blendLeft);
  readBlend("BlendRight", n.blendRight);
  readBlend("BlendTop", n.blendTop);
  readBlend("BlendBottom", n.blendBottom);

  if(auto cw = obj.tryGet("CornerWarp"))
  {
    auto arr = cw->toArray();
    if(arr.Size() == 8)
    {
      n.cornerWarp.topLeft = {arr[0].GetDouble(), arr[1].GetDouble()};
      n.cornerWarp.topRight = {arr[2].GetDouble(), arr[3].GetDouble()};
      n.cornerWarp.bottomLeft = {arr[4].GetDouble(), arr[5].GetDouble()};
      n.cornerWarp.bottomRight = {arr[6].GetDouble(), arr[7].GetDouble()};
    }
  }
}
