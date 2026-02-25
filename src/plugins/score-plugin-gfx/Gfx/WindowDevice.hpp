#pragma once
#include <Gfx/GfxDevice.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QRectF>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsPolygonItem;
class QLabel;
class QStackedWidget;
class QSpinBox;
class QGraphicsView;

namespace score::gfx
{
class Window;
}
namespace Gfx
{

struct WindowOutputSettings
{
  int width{};
  int height{};
  double rate{};
  bool viewportSize{};
  bool vsync{};
};

enum class WindowMode : int
{
  Single = 0,
  Background = 1,
  MultiWindow = 2
};

struct EdgeBlend
{
  float width{0.0f};  // Blend width in UV space (0.0 = no blend, 0.15 = 15% of output)
  float gamma{2.2f};  // Blend curve exponent (1.0 = linear, 2.2 = typical projector)
};

struct CornerWarp
{
  QPointF topLeft{0.0, 0.0};
  QPointF topRight{1.0, 0.0};
  QPointF bottomLeft{0.0, 1.0};
  QPointF bottomRight{1.0, 1.0};

  bool isIdentity() const noexcept
  {
    constexpr double eps = 1e-6;
    return qAbs(topLeft.x()) < eps && qAbs(topLeft.y()) < eps
           && qAbs(topRight.x() - 1.0) < eps && qAbs(topRight.y()) < eps
           && qAbs(bottomLeft.x()) < eps && qAbs(bottomLeft.y() - 1.0) < eps
           && qAbs(bottomRight.x() - 1.0) < eps && qAbs(bottomRight.y() - 1.0) < eps;
  }
};

struct OutputMapping
{
  QRectF sourceRect{0.0, 0.0, 1.0, 1.0}; // UV coords in input texture
  int screenIndex{-1};                     // -1 = default screen
  QPoint windowPosition{0, 0};
  QSize windowSize{1280, 720};
  bool fullscreen{false};

  // Soft-edge blending per side
  EdgeBlend blendLeft;
  EdgeBlend blendRight;
  EdgeBlend blendTop;
  EdgeBlend blendBottom;

  // 4-corner perspective warp (output UV space)
  CornerWarp cornerWarp;
};

class WindowProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("5a181207-7d40-4ad8-814e-879fcdf8cc31")
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx, QWidget*) override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

class SCORE_PLUGIN_GFX_EXPORT WindowDevice final : public GfxOutputDevice
{
  W_OBJECT(WindowDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~WindowDevice();

  score::gfx::Window* window() const noexcept;
  W_SLOT(window)

  void addAddress(const Device::FullAddressSettings& settings) override;
  void setupContextMenu(QMenu&) const override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }
  void disconnect() override;
  bool reconnect() override;

private:
  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

struct WindowSettings
{
  WindowMode mode{WindowMode::Single};
  std::vector<OutputMapping> outputs;
  int inputWidth{1920};
  int inputHeight{1080};
};

// Graphics item for draggable/resizable output mapping quads
class OutputMappingItem final : public QGraphicsRectItem
{
public:
  explicit OutputMappingItem(int index, const QRectF& rect, QGraphicsItem* parent = nullptr);

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

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

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

enum class PreviewContent
{
  Black,
  PerOutputTestCard,
  GlobalTestCard,
  OutputIdentification
};

class PreviewWidget final : public QWidget
{
public:
  explicit PreviewWidget(int index, PreviewContent content, QWidget* parent = nullptr);

  void setOutputIndex(int idx);
  void setPreviewContent(PreviewContent mode);
  void setOutputResolution(QSize sz);
  void setBlend(EdgeBlend left, EdgeBlend right, EdgeBlend top, EdgeBlend bottom);
  void setSourceRect(QRectF rect);
  void setCornerWarp(const CornerWarp& warp);
  void setGlobalTestCard(const QImage& img);

  // Called when fullscreen is toggled via double-click (index, isFullscreen)
  std::function<void(int, bool)> onFullscreenToggled;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
  int m_index{};
  PreviewContent m_content{PreviewContent::Black};
  QSize m_resolution{1280, 720};
  QRectF m_sourceRect{0, 0, 1, 1};
  QImage m_globalTestCard;
  EdgeBlend m_blendLeft;
  EdgeBlend m_blendRight;
  EdgeBlend m_blendTop;
  EdgeBlend m_blendBottom;
  CornerWarp m_cornerWarp;
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
  QGraphicsEllipseItem* m_handles[4]{};  // TL, TR, BL, BR
  QGraphicsPolygonItem* m_quadOutline{};
  std::vector<QGraphicsLineItem*> m_gridLines;

  CornerWarp m_warp;
  double m_canvasSize{200.0};
  bool m_dragging{false};
  int m_dragHandle{-1};
  QPointF m_handleAnchor{};  // scene pos of handle at press
  QPointF m_mouseAnchor{};   // mouse scene pos at press
};

class OutputPreviewWindows final : public QObject
{
public:
  explicit OutputPreviewWindows(QObject* parent = nullptr);
  ~OutputPreviewWindows();

  void syncToMappings(const std::vector<OutputMapping>& mappings);
  void setPreviewContent(PreviewContent mode);
  void setInputResolution(QSize sz);
  void setSyncPositions(bool sync);

  // Called when a preview window's fullscreen state is toggled via double-click
  std::function<void(int, bool)> onFullscreenToggled;

private:
  void closeAll();
  void rebuildGlobalTestCard();

  std::vector<PreviewWidget*> m_windows;
  PreviewContent m_content{PreviewContent::Black};
  QSize m_inputResolution{1920, 1080};
  QImage m_globalTestCard;
  bool m_syncPositions{true};
};

class WindowSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit WindowSettingsWidget(QWidget* parent = nullptr);
  ~WindowSettingsWidget();

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void onModeChanged(int index);
  void onSelectionChanged(int outputIndex);
  void updatePropertiesFromSelection();
  void applyPropertiesToSelection();
  void applySourceRectToSelection();
  void updatePixelLabels();

  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_modeCombo{};
  QStackedWidget* m_stack{};

  // Multi-window UI
  CornerWarpCanvas* m_warpCanvas{};
  OutputMappingCanvas* m_canvas{};
  QSpinBox* m_winPosX{};
  QSpinBox* m_winPosY{};
  QSpinBox* m_winWidth{};
  QSpinBox* m_winHeight{};
  QComboBox* m_screenCombo{};
  QCheckBox* m_fullscreenCheck{};
  QDoubleSpinBox* m_srcX{};
  QDoubleSpinBox* m_srcY{};
  QDoubleSpinBox* m_srcW{};
  QDoubleSpinBox* m_srcH{};
  QLabel* m_srcPosPixelLabel{};
  QLabel* m_srcSizePixelLabel{};
  QSpinBox* m_inputWidth{};
  QSpinBox* m_inputHeight{};

  // Soft-edge blending controls
  QDoubleSpinBox* m_blendLeftW{};
  QDoubleSpinBox* m_blendLeftG{};
  QDoubleSpinBox* m_blendRightW{};
  QDoubleSpinBox* m_blendRightG{};
  QDoubleSpinBox* m_blendTopW{};
  QDoubleSpinBox* m_blendTopG{};
  QDoubleSpinBox* m_blendBottomW{};
  QDoubleSpinBox* m_blendBottomG{};

  int m_selectedOutput{-1};

  // Preview windows
  OutputPreviewWindows* m_preview{};
  QComboBox* m_previewContentCombo{};
  QCheckBox* m_syncPositionsCheck{};
  void syncPreview();
};

}
