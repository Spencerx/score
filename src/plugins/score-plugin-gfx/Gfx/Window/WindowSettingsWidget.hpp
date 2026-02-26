#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsSceneMouseEvent>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <qcheckbox.h>

namespace Gfx
{
class CornerWarpCanvas;
class OutputMappingCanvas;
class OutputPreviewWindows;

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
