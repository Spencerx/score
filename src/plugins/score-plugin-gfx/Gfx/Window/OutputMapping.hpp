#pragma once #pragma once
#include <Gfx/Window/WindowSettings.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace Gfx
{

// Graphics item for draggable/resizable output mapping quads
class OutputMappingItem final : public QGraphicsRectItem
{
public:
  explicit OutputMappingItem(
      int index, const QRectF& rect, QGraphicsItem* parent = nullptr);

  int outputIndex() const noexcept { return m_index; }
  void setOutputIndex(int idx);

  // Per-output window properties stored on the item
  int screenIndex{-1};
  QPoint windowPosition{0, 0};
  QSize windowSize{1280, 720};
  bool fullscreen{false};

  // Soft-edge blending
  EdgeBlend blendLeft;
  EdgeBlend blendRight;
  EdgeBlend blendTop;
  EdgeBlend blendBottom;

  // 4-corner perspective warp
  CornerWarp cornerWarp;

  // Called when item is moved or resized in the canvas
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

  enum BlendHandle
  {
    BlendNone = 0,
    BlendLeft,
    BlendRight,
    BlendTop,
    BlendBottom
  };
  BlendHandle hitTestBlendHandles(const QPointF& pos) const;

  int m_index{};
  int m_resizeEdges{None};
  BlendHandle m_blendHandle{BlendNone};
  QPointF m_dragStart{};
  QRectF m_rectStart{};
  QPointF m_moveAnchorScene{}; // scene pos at press for precision move
  QPointF m_posAtPress{};      // item pos() at press
};
class CornerWarpCanvas final : public QGraphicsView
{
public:
  explicit CornerWarpCanvas(QWidget* parent = nullptr);

  void setWarp(const CornerWarp& warp);
  CornerWarp getWarp() const;
  void resetWarp();

  void setEnabled(bool enabled);

  std::function<void()> onChanged;

protected:
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  void rebuildItems();
  void updateLinesAndGrid();

  QGraphicsScene m_scene;
  QGraphicsRectItem* m_border{};
  QGraphicsEllipseItem* m_handles[4]{}; // TL, TR, BL, BR
  QGraphicsPolygonItem* m_quadOutline{};
  std::vector<QGraphicsLineItem*> m_gridLines;

  CornerWarp m_warp;
  double m_canvasSize{200.0};
  bool m_dragging{false};
  int m_dragHandle{-1};
  QPointF m_handleAnchor{}; // scene pos of handle at press
  QPointF m_mouseAnchor{};  // mouse scene pos at press
};
class OutputMappingCanvas final : public QGraphicsView
{
public:
  explicit OutputMappingCanvas(QWidget* parent = nullptr);
  ~OutputMappingCanvas();

  void setMappings(const std::vector<OutputMapping>& mappings);
  std::vector<OutputMapping> getMappings() const;

  void addOutput();
  void removeSelectedOutput();

  void updateAspectRatio(int inputWidth, int inputHeight);

  double canvasWidth() const noexcept { return m_canvasWidth; }
  double canvasHeight() const noexcept { return m_canvasHeight; }

  // Signal-like: call this when selection changes
  std::function<void(int)> onSelectionChanged;
  // Called when a mapping item's geometry changes (move/resize)
  std::function<void(int)> onItemGeometryChanged;

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  void setupItemCallbacks(OutputMappingItem* item);
  QGraphicsScene m_scene;
  QGraphicsRectItem* m_border{};
  double m_canvasWidth{400.0};
  double m_canvasHeight{300.0};
};
}
