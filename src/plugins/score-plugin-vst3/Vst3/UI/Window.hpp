#pragma once
#include <Media/Effect/Settings/Model.hpp>
#include <Vst3/EffectModel.hpp>
#include <Vst3/UI/WindowContainer.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/widgets/PluginWindow.hpp>

#include <QResizeEvent>
#include <QWindow>

#include <pluginterfaces/gui/iplugview.h>

namespace vst3
{

WindowContainer createVstWindowContainer(
    Window& parentWindow, const Model& e, const score::DocumentContext& ctx);

class Window : public score::PluginWindow
{
  Model& m_model;
  WindowContainer container;

public:
  Window(Model& e, const score::DocumentContext& ctx, QWidget* parent);
  ~Window();

  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
};
}
