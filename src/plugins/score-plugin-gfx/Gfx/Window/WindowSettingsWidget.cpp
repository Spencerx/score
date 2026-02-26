#include "WindowSettingsWidget.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/Window/OutputMapping.hpp>
#include <Gfx/Window/OutputPreview.hpp>
#include <Gfx/WindowDevice.hpp>
namespace Gfx
{

WindowSettingsWidget::WindowSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  m_modeCombo = new QComboBox;
  m_modeCombo->addItem(tr("Single Window"));
  m_modeCombo->addItem(tr("Background"));
  m_modeCombo->addItem(tr("Multi-Window Mapping"));
  layout->addRow(tr("Mode"), m_modeCombo);

  m_stack = new QStackedWidget;

  // Page 0: Single (empty)
  m_stack->addWidget(new QWidget);

  // Page 1: Background (empty)
  m_stack->addWidget(new QWidget);

  // Page 2: Multi-window
  {
    auto* multiWidget = new QWidget;
    auto* multiLayout = new QVBoxLayout(multiWidget);

    // Input texture resolution (for pixel label display)
    {
      auto* resLayout = new QHBoxLayout;
      resLayout->addWidget(new QLabel(tr("Input Resolution")));
      m_inputWidth = new QSpinBox;
      m_inputWidth->setRange(1, 16384);
      m_inputWidth->setValue(1920);
      m_inputHeight = new QSpinBox;
      m_inputHeight->setRange(1, 16384);
      m_inputHeight->setValue(1080);
      resLayout->addWidget(m_inputWidth);
      resLayout->addWidget(new QLabel(QStringLiteral("x")));
      resLayout->addWidget(m_inputHeight);
      multiLayout->addLayout(resLayout);
    }

    // Side-by-side: warp editor (left) + source rect canvas (right)
    auto* canvasRow = new QHBoxLayout;
    m_warpCanvas = new CornerWarpCanvas;
    m_warpCanvas->setEnabled(false);
    canvasRow->addWidget(m_warpCanvas);
    m_canvas = new OutputMappingCanvas;
    canvasRow->addWidget(m_canvas, 1);
    multiLayout->addLayout(canvasRow);

    // Add/Remove buttons
    auto* btnLayout = new QHBoxLayout;
    auto* addBtn = new QPushButton(tr("Add Output"));
    auto* removeBtn = new QPushButton(tr("Remove Selected"));
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    multiLayout->addLayout(btnLayout);

    // Preview content combo
    static int s_lastPreviewContent = 0;
    m_previewContentCombo = new QComboBox;
    m_previewContentCombo->addItem(tr("Black"));
    m_previewContentCombo->addItem(tr("Per-Output Test Card"));
    m_previewContentCombo->addItem(tr("Global Test Card"));
    m_previewContentCombo->addItem(tr("Output ID"));
    m_previewContentCombo->setCurrentIndex(s_lastPreviewContent);
    btnLayout->addWidget(new QLabel(tr("Preview:")));
    btnLayout->addWidget(m_previewContentCombo);

    auto* resetWarpBtn = new QPushButton(tr("Reset Warp"));
    btnLayout->addWidget(resetWarpBtn);
    connect(resetWarpBtn, &QPushButton::clicked, this, [this] {
      if(m_warpCanvas)
        m_warpCanvas->resetWarp();
    });

    m_syncPositionsCheck = new QCheckBox(tr("Sync Positions"));
    m_syncPositionsCheck->setChecked(false);
    m_syncPositionsCheck->setToolTip(
        tr("Synchronize preview window positions and sizes with the output mappings"));
    btnLayout->addWidget(m_syncPositionsCheck);

    connect(
        m_previewContentCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](int idx) {
      s_lastPreviewContent = idx;
      if(m_preview)
        m_preview->setPreviewContent(static_cast<PreviewContent>(idx));
    });

    connect(m_syncPositionsCheck, &QCheckBox::toggled, this, [this](bool checked) {
      if(m_preview)
        m_preview->setSyncPositions(checked);
    });

    connect(addBtn, &QPushButton::clicked, this, [this] {
      m_canvas->addOutput();

      // Set initial window properties from source rect x input resolution
      // (don't rely on deferred selectionChanged signal for auto-match)
      int inW = m_inputWidth->value();
      int inH = m_inputHeight->value();
      OutputMappingItem* newest = nullptr;
      for(auto* item : m_canvas->scene()->items())
        if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
          if(!newest || mi->outputIndex() > newest->outputIndex())
            newest = mi;
      if(newest)
      {
        auto r = newest->mapRectToScene(newest->rect());
        double srcX = r.x() / m_canvas->canvasWidth();
        double srcY = r.y() / m_canvas->canvasHeight();
        double srcW = r.width() / m_canvas->canvasWidth();
        double srcH = r.height() / m_canvas->canvasHeight();
        newest->windowPosition = QPoint(int(srcX * inW), int(srcY * inH));
        newest->windowSize = QSize(qMax(1, int(srcW * inW)), qMax(1, int(srcH * inH)));
      }

      syncPreview();
    });
    connect(removeBtn, &QPushButton::clicked, this, [this] {
      m_canvas->removeSelectedOutput();
      m_selectedOutput = -1;
      syncPreview();
    });

    // Properties for selected output
    auto* propGroup = new QGroupBox(tr("Selected Output Properties"));
    auto* propLayout = new QFormLayout(propGroup);

    // Source rect (UV coordinates in the input texture)
    m_srcX = new QDoubleSpinBox;
    m_srcX->setRange(0.0, 1.0);
    m_srcX->setSingleStep(0.01);
    m_srcX->setDecimals(3);
    m_srcY = new QDoubleSpinBox;
    m_srcY->setRange(0.0, 1.0);
    m_srcY->setSingleStep(0.01);
    m_srcY->setDecimals(3);
    auto* srcPosLayout = new QHBoxLayout;
    srcPosLayout->addWidget(m_srcX);
    srcPosLayout->addWidget(m_srcY);
    m_srcPosPixelLabel = new QLabel;
    srcPosLayout->addWidget(m_srcPosPixelLabel);
    propLayout->addRow(tr("Source UV Pos"), srcPosLayout);

    m_srcW = new QDoubleSpinBox;
    m_srcW->setRange(0.01, 1.0);
    m_srcW->setSingleStep(0.01);
    m_srcW->setDecimals(3);
    m_srcW->setValue(1.0);
    m_srcH = new QDoubleSpinBox;
    m_srcH->setRange(0.01, 1.0);
    m_srcH->setSingleStep(0.01);
    m_srcH->setDecimals(3);
    m_srcH->setValue(1.0);
    auto* srcSizeLayout = new QHBoxLayout;
    srcSizeLayout->addWidget(m_srcW);
    srcSizeLayout->addWidget(m_srcH);
    m_srcSizePixelLabel = new QLabel;
    srcSizeLayout->addWidget(m_srcSizePixelLabel);
    propLayout->addRow(tr("Source UV Size"), srcSizeLayout);

    // Output window properties
    m_screenCombo = new QComboBox;
    m_screenCombo->addItem(tr("Default"));
    for(auto* screen : qApp->screens())
      m_screenCombo->addItem(screen->name());
    propLayout->addRow(tr("Screen"), m_screenCombo);

    m_winPosX = new QSpinBox;
    m_winPosX->setRange(0, 16384);
    m_winPosY = new QSpinBox;
    m_winPosY->setRange(0, 16384);
    auto* posLayout = new QHBoxLayout;
    posLayout->addWidget(m_winPosX);
    posLayout->addWidget(m_winPosY);
    propLayout->addRow(tr("Window Pos"), posLayout);

    m_winWidth = new QSpinBox;
    m_winWidth->setRange(1, 16384);
    m_winWidth->setValue(1280);
    m_winHeight = new QSpinBox;
    m_winHeight->setRange(1, 16384);
    m_winHeight->setValue(720);
    auto* sizeLayout = new QHBoxLayout;
    sizeLayout->addWidget(m_winWidth);
    sizeLayout->addWidget(m_winHeight);
    propLayout->addRow(tr("Window Size"), sizeLayout);

    m_fullscreenCheck = new QCheckBox;
    propLayout->addRow(tr("Fullscreen"), m_fullscreenCheck);

    // Soft-edge blending controls
    auto* blendGroup = new QGroupBox(tr("Soft-Edge Blending"));
    auto* blendLayout = new QFormLayout(blendGroup);

    auto makeBlendRow = [&](const QString& label, QDoubleSpinBox*& widthSpin,
                            QDoubleSpinBox*& gammaSpin) {
      widthSpin = new QDoubleSpinBox;
      widthSpin->setRange(0.0, 0.5);
      widthSpin->setSingleStep(0.01);
      widthSpin->setDecimals(3);
      widthSpin->setValue(0.0);
      gammaSpin = new QDoubleSpinBox;
      gammaSpin->setRange(0.1, 4.0);
      gammaSpin->setSingleStep(0.1);
      gammaSpin->setDecimals(2);
      gammaSpin->setValue(2.2);
      auto* row = new QHBoxLayout;
      row->addWidget(new QLabel(tr("Width")));
      row->addWidget(widthSpin);
      row->addWidget(new QLabel(tr("Gamma")));
      row->addWidget(gammaSpin);
      row->addStretch(1);
      blendLayout->addRow(label, row);
    };

    makeBlendRow(tr("Left"), m_blendLeftW, m_blendLeftG);
    makeBlendRow(tr("Right"), m_blendRightW, m_blendRightG);
    makeBlendRow(tr("Top"), m_blendTopW, m_blendTopG);
    makeBlendRow(tr("Bottom"), m_blendBottomW, m_blendBottomG);

    propLayout->addRow(blendGroup);

    propGroup->setEnabled(false);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidget(propGroup);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    multiLayout->addWidget(scrollArea, 1);

    m_canvas->onSelectionChanged = [this, propGroup](int idx) {
      m_selectedOutput = idx;
      propGroup->setEnabled(idx >= 0);
      if(m_warpCanvas)
        m_warpCanvas->setEnabled(idx >= 0);
      if(idx >= 0)
        updatePropertiesFromSelection();
      else if(m_warpCanvas)
        m_warpCanvas->setWarp(CornerWarp{});
    };

    m_canvas->onItemGeometryChanged = [this](int idx) {
      if(idx == m_selectedOutput)
        updatePropertiesFromSelection();
      syncPreview();
    };

    // Wire corner warp canvas changes back to the selected item
    m_warpCanvas->onChanged = [this] {
      if(!m_canvas || m_selectedOutput < 0)
        return;
      for(auto* item : m_canvas->scene()->items())
      {
        if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
        {
          if(mi->outputIndex() == m_selectedOutput)
          {
            mi->cornerWarp = m_warpCanvas->getWarp();
            syncPreview();
            return;
          }
        }
      }
    };

    // Wire window property changes back to the selected canvas item
    auto applyProps = [this] { applyPropertiesToSelection(); };
    connect(
        m_screenCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
        applyProps);
    connect(m_winPosX, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winPosY, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winWidth, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winHeight, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_fullscreenCheck, &QCheckBox::toggled, this, applyProps);
    connect(
        m_blendLeftW, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);
    connect(
        m_blendLeftG, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);
    connect(
        m_blendRightW, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);
    connect(
        m_blendRightG, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);
    connect(
        m_blendTopW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(
        m_blendTopG, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(
        m_blendBottomW, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);
    connect(
        m_blendBottomG, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);

    // Wire source rect spinboxes to update canvas item geometry
    auto applySrc = [this] { applySourceRectToSelection(); };
    connect(m_srcX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcH, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);

    // Update pixel labels when UV or input resolution changes
    auto updatePx = [this] { updatePixelLabels(); };
    connect(m_srcX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcH, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_inputWidth, qOverload<int>(&QSpinBox::valueChanged), this, updatePx);
    connect(m_inputHeight, qOverload<int>(&QSpinBox::valueChanged), this, updatePx);
    connect(
        m_screenCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, updatePx);

    // Update canvas aspect ratio and preview test card when input resolution changes
    auto updateAR = [this] {
      m_canvas->updateAspectRatio(m_inputWidth->value(), m_inputHeight->value());
      if(m_preview)
        m_preview->setInputResolution(
            QSize(m_inputWidth->value(), m_inputHeight->value()));
    };
    connect(m_inputWidth, qOverload<int>(&QSpinBox::valueChanged), this, updateAR);
    connect(m_inputHeight, qOverload<int>(&QSpinBox::valueChanged), this, updateAR);

    // Set initial aspect ratio
    m_canvas->updateAspectRatio(m_inputWidth->value(), m_inputHeight->value());

    m_stack->addWidget(multiWidget);
  }

  layout->addRow(m_stack);
  m_deviceNameEdit->setText("window");

  connect(
      m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
      &WindowSettingsWidget::onModeChanged);

  setLayout(layout);
}

WindowSettingsWidget::~WindowSettingsWidget()
{
  if(this->m_modeCombo->currentIndex() == (int)WindowMode::MultiWindow)
  {
    if(auto doc = score::GUIAppContext().currentDocument())
    {
      auto& p = doc->plugin<Explorer::DeviceDocumentPlugin>();
      auto& dl = p.list();
      if(auto* self = dl.findDevice(this->m_deviceNameEdit->text()))
      {
        QMetaObject::invokeMethod(
            safe_cast<WindowDevice*>(self), &WindowDevice::reconnect);
      }
    }
  }
}

void WindowSettingsWidget::onModeChanged(int index)
{
  m_stack->setCurrentIndex(index);

  if(index != (int)WindowMode::MultiWindow)
  {
    // Destroy preview when leaving multi-window mode
    delete m_preview;
    m_preview = nullptr;
  }
  else
  {
    // Entering multi-window mode: always create preview
    if(!m_preview)
    {
      m_preview = new OutputPreviewWindows(this);
      m_preview->onFullscreenToggled = [this](int idx, bool fs) {
        // Update the mapping item
        if(m_canvas)
        {
          for(auto* item : m_canvas->scene()->items())
          {
            if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
            {
              if(mi->outputIndex() == idx)
              {
                mi->fullscreen = fs;
                break;
              }
            }
          }
        }
        // Update checkbox if this is the selected output
        if(idx == m_selectedOutput && m_fullscreenCheck)
        {
          const QSignalBlocker b(m_fullscreenCheck);
          m_fullscreenCheck->setChecked(fs);
        }
      };
      if(m_previewContentCombo)
        m_preview->setPreviewContent(
            static_cast<PreviewContent>(m_previewContentCombo->currentIndex()));
      if(m_syncPositionsCheck)
        m_preview->setSyncPositions(m_syncPositionsCheck->isChecked());
      m_preview->setInputResolution(
          QSize(m_inputWidth->value(), m_inputHeight->value()));
      syncPreview();
    }
  }
}

void WindowSettingsWidget::updatePropertiesFromSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        // Block signals to avoid feedback loops when setting multiple spinboxes
        const QSignalBlocker b1(m_screenCombo);
        const QSignalBlocker b2(m_winPosX);
        const QSignalBlocker b3(m_winPosY);
        const QSignalBlocker b4(m_winWidth);
        const QSignalBlocker b5(m_winHeight);
        const QSignalBlocker b6(m_fullscreenCheck);
        const QSignalBlocker b7(m_srcX);
        const QSignalBlocker b8(m_srcY);
        const QSignalBlocker b9(m_srcW);
        const QSignalBlocker b10(m_srcH);

        // Update source rect from canvas item geometry
        auto sceneRect = mi->mapRectToScene(mi->rect());
        m_srcX->setValue(sceneRect.x() / m_canvas->canvasWidth());
        m_srcY->setValue(sceneRect.y() / m_canvas->canvasHeight());
        m_srcW->setValue(sceneRect.width() / m_canvas->canvasWidth());
        m_srcH->setValue(sceneRect.height() / m_canvas->canvasHeight());

        // Update window properties
        m_screenCombo->setCurrentIndex(
            mi->screenIndex + 1); // +1 because index 0 is "Default"
        m_winPosX->setValue(mi->windowPosition.x());
        m_winPosY->setValue(mi->windowPosition.y());
        m_winWidth->setValue(mi->windowSize.width());
        m_winHeight->setValue(mi->windowSize.height());
        m_fullscreenCheck->setChecked(mi->fullscreen);

        {
          const QSignalBlocker b11(m_blendLeftW);
          const QSignalBlocker b12(m_blendLeftG);
          const QSignalBlocker b13(m_blendRightW);
          const QSignalBlocker b14(m_blendRightG);
          const QSignalBlocker b15(m_blendTopW);
          const QSignalBlocker b16(m_blendTopG);
          const QSignalBlocker b17(m_blendBottomW);
          const QSignalBlocker b18(m_blendBottomG);

          m_blendLeftW->setValue(mi->blendLeft.width);
          m_blendLeftG->setValue(mi->blendLeft.gamma);
          m_blendRightW->setValue(mi->blendRight.width);
          m_blendRightG->setValue(mi->blendRight.gamma);
          m_blendTopW->setValue(mi->blendTop.width);
          m_blendTopG->setValue(mi->blendTop.gamma);
          m_blendBottomW->setValue(mi->blendBottom.width);
          m_blendBottomG->setValue(mi->blendBottom.gamma);
        }

        if(m_warpCanvas)
          m_warpCanvas->setWarp(mi->cornerWarp);

        updatePixelLabels();
        return;
      }
    }
  }
}

void WindowSettingsWidget::applyPropertiesToSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        mi->screenIndex
            = m_screenCombo->currentIndex() - 1; // -1 because index 0 is "Default"
        mi->windowPosition = QPoint(m_winPosX->value(), m_winPosY->value());
        mi->windowSize = QSize(m_winWidth->value(), m_winHeight->value());
        mi->fullscreen = m_fullscreenCheck->isChecked();

        mi->blendLeft = {(float)m_blendLeftW->value(), (float)m_blendLeftG->value()};
        mi->blendRight = {(float)m_blendRightW->value(), (float)m_blendRightG->value()};
        mi->blendTop = {(float)m_blendTopW->value(), (float)m_blendTopG->value()};
        mi->blendBottom
            = {(float)m_blendBottomW->value(), (float)m_blendBottomG->value()};
        mi->update(); // Repaint to show blend zones
        syncPreview();
        return;
      }
    }
  }
}

void WindowSettingsWidget::applySourceRectToSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        QRectF newSceneRect(
            m_srcX->value() * m_canvas->canvasWidth(),
            m_srcY->value() * m_canvas->canvasHeight(),
            m_srcW->value() * m_canvas->canvasWidth(),
            m_srcH->value() * m_canvas->canvasHeight());

        // Temporarily disable onChanged to avoid feedback loop
        auto savedCallback = std::move(mi->onChanged);
        mi->setPos(0, 0);
        mi->setRect(newSceneRect);
        mi->onChanged = std::move(savedCallback);
        syncPreview();
        return;
      }
    }
  }
}

void WindowSettingsWidget::updatePixelLabels()
{
  if(!m_inputWidth || !m_inputHeight)
    return;

  int w = m_inputWidth->value();
  int h = m_inputHeight->value();

  int pxX = int(m_srcX->value() * w);
  int pxY = int(m_srcY->value() * h);
  int pxW = qMax(1, int(m_srcW->value() * w));
  int pxH = qMax(1, int(m_srcH->value() * h));

  if(m_srcPosPixelLabel)
    m_srcPosPixelLabel->setText(QStringLiteral("(%1, %2 px)").arg(pxX).arg(pxY));

  if(m_srcSizePixelLabel)
    m_srcSizePixelLabel->setText(QStringLiteral("%1 x %2 px").arg(pxW).arg(pxH));

  // Auto-match window pos/size when screen is "Default" (combo index 0 = screenIndex -1)
  if(m_screenCombo && m_screenCombo->currentIndex() == 0 && m_selectedOutput >= 0)
  {
    const QSignalBlocker bx(m_winPosX);
    const QSignalBlocker by(m_winPosY);
    const QSignalBlocker bw(m_winWidth);
    const QSignalBlocker bh(m_winHeight);

    m_winPosX->setValue(pxX);
    m_winPosY->setValue(pxY);
    m_winWidth->setValue(pxW);
    m_winHeight->setValue(pxH);

    applyPropertiesToSelection();
  }
}

void WindowSettingsWidget::syncPreview()
{
  if(m_preview && m_canvas)
    m_preview->syncToMappings(m_canvas->getMappings());
}

Device::DeviceSettings WindowSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings set;
  set.mode = (WindowMode)m_modeCombo->currentIndex();

  if(set.mode == WindowMode::MultiWindow && m_canvas)
  {
    // Apply any pending property changes before reading
    const_cast<WindowSettingsWidget*>(this)->applyPropertiesToSelection();
    set.outputs = m_canvas->getMappings();
  }

  if(m_inputWidth && m_inputHeight)
  {
    set.inputWidth = m_inputWidth->value();
    set.inputHeight = m_inputHeight->value();
  }

  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void WindowSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  if(settings.deviceSpecificSettings.canConvert<WindowSettings>())
  {
    const auto set = settings.deviceSpecificSettings.value<WindowSettings>();
    m_modeCombo->setCurrentIndex((int)set.mode);
    m_stack->setCurrentIndex((int)set.mode);

    if(m_inputWidth && m_inputHeight)
    {
      m_inputWidth->setValue(set.inputWidth);
      m_inputHeight->setValue(set.inputHeight);
    }

    if(this->m_modeCombo->currentIndex() == (int)WindowMode::MultiWindow)
    {
      if(auto doc = score::GUIAppContext().currentDocument())
      {
        auto& p = doc->plugin<Explorer::DeviceDocumentPlugin>();
        auto& dl = p.list();
        if(auto* self = dl.findDevice(settings.name))
        {
          safe_cast<WindowDevice*>(self)->disconnect();
        }
      }
    }

    if(set.mode == WindowMode::MultiWindow && m_canvas)
    {
      m_canvas->setMappings(set.outputs);
      syncPreview();
    }
  }
}
}
