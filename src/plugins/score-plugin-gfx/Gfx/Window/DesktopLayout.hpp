#pragma once
#include <Gfx/Window/WindowSettings.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <functional>
#include <vector>

namespace Gfx
{

class DesktopLayoutCanvas;

// Represents an output window positioned on the virtual desktop
class DesktopLayoutItem final : public QGraphicsRectItem
{
public:
  explicit DesktopLayoutItem(
      int index, const QRectF& rect, DesktopLayoutCanvas* canvas,
      QGraphicsItem* parent = nullptr);

  int outputIndex() const noexcept { return m_index; }
  void setOutputIndex(int idx);

  OutputLockMode lockMode{OutputLockMode::Free};
  double aspectRatio{16.0 / 9.0}; // width/height, used for AspectRatio lock mode

  void applyLockedState();

  std::function<void()> onChanged;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  enum ResizeEdge
  {
    None = 0,
    Left = 1,
    Right = 2,
    Top = 4,
    Bottom = 8
  };
  int hitTestEdges(const QPointF& pos) const;

  int m_index{};
  int m_resizeEdges{None};
  QPointF m_dragStart{};
  QRectF m_rectStart{};
  QPointF m_moveAnchorScene{};
  QPointF m_posAtPress{};
  DesktopLayoutCanvas* m_canvas{};
};

// QGraphicsView that displays OS screens and draggable output window items
class DesktopLayoutCanvas final : public QGraphicsView
{
public:
  explicit DesktopLayoutCanvas(QWidget* parent = nullptr);
  ~DesktopLayoutCanvas();

  void refreshScreens();

  void setWindowItems(const std::vector<OutputMapping>& mappings);
  void addOutput(QPoint pos, QSize size);
  void removeOutput(int index);
  void updateItem(int index, QPoint pos, QSize size);
  void selectItem(int index);

  // Desktop coords <-> scene coords
  QPointF desktopToScene(QPointF desktopPos) const;
  QPointF sceneToDesktop(QPointF scenePos) const;
  QSizeF desktopSizeToScene(QSize desktopSize) const;
  QSize sceneSizeToDesktop(QSizeF sceneSize) const;

  // Returns the screen index with largest overlap for the given desktop rect, or -1
  int detectScreen(QRect windowRect) const;

  bool snapEnabled() const noexcept { return m_snapEnabled; }
  void setSnapEnabled(bool enabled);

  // Returns snap-adjusted position for a moving item (in scene coords)
  QPointF snapPosition(const DesktopLayoutItem* item, QPointF proposedPos) const;

  std::function<void(int)> onSelectionChanged;
  std::function<void(int)> onItemGeometryChanged;

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  void rebuildScreenRects();
  void setupItemCallbacks(DesktopLayoutItem* item);

  QGraphicsScene m_scene;
  std::vector<QGraphicsRectItem*> m_screenRects;
  std::vector<QGraphicsSimpleTextItem*> m_screenLabels;

  QRectF m_virtualDesktopBounds; // in desktop coords
  double m_scaleFactor{1.0};
  QPointF m_sceneOffset; // scene offset = -bounds.topLeft() * scale
  bool m_snapEnabled{true};
};

}
