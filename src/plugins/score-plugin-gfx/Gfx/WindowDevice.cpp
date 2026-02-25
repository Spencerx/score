#include "WindowDevice.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/graphics/BackgroundRenderer.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/invoke.hpp>

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

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::WindowDevice)

namespace Gfx
{
static score::gfx::ScreenNode* createScreenNode()
{
  const auto& settings = score::AppContext().applicationSettings;
  const auto& gfx_settings = score::AppContext().settings<Gfx::Settings::Model>();

  auto make_configuration = [&] {
    score::gfx::OutputNode::Configuration conf;
    double rate = gfx_settings.getRate();
    if(rate > 0)
      conf = {.manualRenderingRate = 1000. / rate, .supportsVSync = true};
    else
      conf = {.manualRenderingRate = {}, .supportsVSync = true};
    return conf;
  };

  auto node = new score::gfx::ScreenNode{
      make_configuration(), false, (settings.autoplay || !settings.gui)};

  QObject::connect(
      &gfx_settings, &Gfx::Settings::Model::RateChanged, node,
      [node, make_configuration] { node->setConfiguration(make_configuration()); });

  return node;
}

class window_device : public ossia::net::device_base
{
  score::gfx::ScreenNode* m_screen{};
  gfx_node_base m_root;
  QObject m_qtContext;

  ossia::net::parameter_base* scaled_win{};
  ossia::net::parameter_base* abs_win{};
  ossia::net::parameter_base* abs_tablet_win{};
  ossia::net::parameter_base* size_param{};
  ossia::net::parameter_base* rendersize_param{};

  void update_viewport()
  {
    auto v = rendersize_param->value();
    if(auto val = v.target<ossia::vec2f>())
    {
      auto dom = abs_win->get_domain();
      if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
      {
        ossia::set_max(dom, *val);
        abs_win->set_domain(std::move(dom));
        abs_tablet_win->set_domain(std::move(dom));
      }
      else
      {
        // Use the normal size
        v = size_param->value();
        if(auto val = v.target<ossia::vec2f>())
        {
          auto dom = abs_win->get_domain();
          if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
          {
            ossia::set_max(dom, *val);
            abs_win->set_domain(std::move(dom));
            abs_tablet_win->set_domain(std::move(dom));
          }
        }
      }
    }
    else
    {
      v = size_param->value();
      if(auto val = v.target<ossia::vec2f>())
      {
        auto dom = abs_win->get_domain();
        if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
        {
          ossia::set_max(dom, *val);
          abs_win->set_domain(std::move(dom));
          abs_tablet_win->set_domain(std::move(dom));
        }
      }
    }
  }

public:
  ~window_device()
  {
    if(auto w = m_screen->window())
      w->close();

    m_screen->onWindowMove = [](QPointF) {};
    m_screen->onMouseMove = [](QPointF, QPointF) {};
    m_screen->onTabletMove = [](QTabletEvent*) {};
    m_screen->onKey = [](int, const QString&) {};
    m_screen->onKeyRelease = [](int, const QString&) { };
    m_screen->onFps = [](float) { };
    m_protocol->stop();

    m_root.clear_children();

    m_protocol.reset();
  }

  window_device(std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_screen{createScreenNode()}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_screen, name}
  {
    this->m_capabilities.change_tree = true;
    m_screen->setTitle(QString::fromStdString(name));

    {
      auto screen_node
          = std::make_unique<ossia::net::generic_node>("screen", *this, m_root);
      auto screen_param = screen_node->create_parameter(ossia::val_type::STRING);
      screen_param->set_domain(ossia::make_domain(int(0), int(100)));
      screen_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<int>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, scr = *val] {
            const auto& cur_screens = qApp->screens();
            if(ossia::valid_index(scr, cur_screens))
            {
              screen->setScreen(cur_screens[scr]);
            }
          });
        }
        else if(auto val = v.target<std::string>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, scr = *val] {
            const auto& cur_screens = qApp->screens();
            for(auto s : cur_screens)
            {
              if(s->name() == scr.c_str())
              {
                screen->setScreen(s);
                break;
              }
            }
          });
        }
      });
      m_root.add_child(std::move(screen_node));
    }

    {
      struct move_window_lock
      {
        bool locked{};
      };
      auto lock = std::make_shared<move_window_lock>();

      auto pos_node
          = std::make_unique<ossia::net::generic_node>("position", *this, m_root);
      auto pos_param = pos_node->create_parameter(ossia::val_type::VEC2F);
      pos_param->push_value(
          ossia::vec2f{100.f, 100.f}); // FIXME Try to detect center of screen ?
      pos_param->add_callback([this, lock](const ossia::value& v) {
        if(lock->locked)
          return;
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val, lock] {
            screen->setPosition({(int)v[0], (int)v[1]});
          });
        }
      });

      m_screen->onWindowMove = [this, pos_param, lock](QPointF pos) {
        if(lock->locked)
          return;
        if(const auto& w = m_screen->window())
        {
          lock->locked = true;
          pos_param->set_value(ossia::vec2f{float(pos.x()), float(pos.y())});
          lock->locked = false;
        }
      };
      m_root.add_child(std::move(pos_node));
    }

    // Mouse input
    {
      auto node = std::make_unique<ossia::net::generic_node>("cursor", *this, m_root);
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("scaled", *this, *node);
        scaled_win = scale_node->create_parameter(ossia::val_type::VEC2F);
        scaled_win->set_domain(ossia::make_domain(0.f, 1.f));
        scaled_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(scale_node));
      }
      {
        auto abs_node
            = std::make_unique<ossia::net::generic_node>("absolute", *this, *node);
        abs_win = abs_node->create_parameter(ossia::val_type::VEC2F);
        abs_win->set_domain(
            ossia::make_domain(ossia::vec2f{0.f, 0.f}, ossia::vec2f{1280, 270.f}));
        abs_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(abs_node));
      }
      {
        auto visible
            = std::make_unique<ossia::net::generic_node>("visible", *this, *node);
        auto param = visible->create_parameter(ossia::val_type::BOOL);
        param->add_callback([this](const ossia::value& v) {
          if(auto val = v.target<bool>())
          {
            ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
              screen->setCursor(v);
            });
          }
        });
        node->add_child(std::move(visible));
      }

      m_screen->onMouseMove = [this](QPointF screen, QPointF win) {
        if(const auto& w = m_screen->window())
        {
          auto sz = w->size();
          scaled_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          abs_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
        }
      };

      m_root.add_child(std::move(node));
    }

    // Tablet input
    ossia::net::parameter_base* scaled_tablet_win{};
    {
      auto node = std::make_unique<ossia::net::generic_node>("tablet", *this, m_root);
      ossia::net::parameter_base* tablet_pressure{};
      ossia::net::parameter_base* tablet_z{};
      ossia::net::parameter_base* tablet_tan{};
      ossia::net::parameter_base* tablet_rot{};
      ossia::net::parameter_base* tablet_tilt_x{};
      ossia::net::parameter_base* tablet_tilt_y{};
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("scaled", *this, *node);
        scaled_tablet_win = scale_node->create_parameter(ossia::val_type::VEC2F);
        scaled_tablet_win->set_domain(ossia::make_domain(0.f, 1.f));
        scaled_tablet_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(scale_node));
      }
      {
        auto abs_node
            = std::make_unique<ossia::net::generic_node>("absolute", *this, *node);
        abs_tablet_win = abs_node->create_parameter(ossia::val_type::VEC2F);
        abs_tablet_win->set_domain(
            ossia::make_domain(ossia::vec2f{0.f, 0.f}, ossia::vec2f{1280, 270.f}));
        abs_tablet_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(abs_node));
      }
      {
        auto scale_node = std::make_unique<ossia::net::generic_node>("z", *this, *node);
        tablet_z = scale_node->create_parameter(ossia::val_type::INT);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("pressure", *this, *node);
        tablet_pressure = scale_node->create_parameter(ossia::val_type::FLOAT);
        //tablet_pressure->set_domain(ossia::make_domain(0.f, 1.f));
        //tablet_pressure->push_value(0.f);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tangential", *this, *node);
        tablet_tan = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tan->set_domain(ossia::make_domain(-1.f, 1.f));
        //tablet_tan->push_value(0.f);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("rotation", *this, *node);
        tablet_rot = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_rot->set_unit(ossia::degree_u{});
        tablet_rot->set_domain(ossia::make_domain(-180.f, 180.f));
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tilt_x", *this, *node);
        tablet_tilt_x = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tilt_x->set_domain(ossia::make_domain(-60.f, 60.f));
        tablet_tilt_x->set_unit(ossia::degree_u{});
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tilt_y", *this, *node);
        tablet_tilt_y = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tilt_y->set_domain(ossia::make_domain(-60.f, 60.f));
        tablet_tilt_y->set_unit(ossia::degree_u{});
        node->add_child(std::move(scale_node));
      }

      m_screen->onTabletMove = [=, this](QTabletEvent* ev) {
        if(const auto& w = m_screen->window())
        {
          const auto sz = w->size();
          const auto win = ev->position();
          scaled_tablet_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          abs_tablet_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
          tablet_pressure->push_value(ev->pressure());
          tablet_tan->push_value(ev->tangentialPressure());
          tablet_rot->push_value(ev->rotation());
          tablet_z->push_value(ev->z());
          tablet_tilt_x->push_value(ev->xTilt());
          tablet_tilt_y->push_value(ev->yTilt());
        }
      };

      m_root.add_child(std::move(node));
    }

    {
      auto size_node = std::make_unique<ossia::net::generic_node>("size", *this, m_root);
      size_param = size_node->create_parameter(ossia::val_type::VEC2F);
      size_param->push_value(ossia::vec2f{1280.f, 720.f});
      size_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setSize({(int)v[0], (int)v[1]});
          });

          update_viewport();
        }
      });

      m_root.add_child(std::move(size_node));
    }

    {
      auto size_node
          = std::make_unique<ossia::net::generic_node>("rendersize", *this, m_root);
      ossia::net::set_description(
          *size_node, "Set to [0, 0] to use the viewport's size");

      rendersize_param = size_node->create_parameter(ossia::val_type::VEC2F);
      rendersize_param->push_value(ossia::vec2f{0.f, 0.f});
      rendersize_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setRenderSize({(int)v[0], (int)v[1]});
          });

          update_viewport();
        }
      });

      m_root.add_child(std::move(size_node));
    }

    // Keyboard input
    {
      auto node = std::make_unique<ossia::net::generic_node>("key", *this, m_root);
      {
        auto press_node
            = std::make_unique<ossia::net::generic_node>("press", *this, *node);
        ossia::net::parameter_base* press_param{};
        ossia::net::parameter_base* text_param{};
        {
          auto code_node
              = std::make_unique<ossia::net::generic_node>("code", *this, *press_node);
          press_param = code_node->create_parameter(ossia::val_type::INT);
          press_param->push_value(ossia::vec2f{0.f, 0.f});
          press_node->add_child(std::move(code_node));
        }
        {
          auto text_node
              = std::make_unique<ossia::net::generic_node>("text", *this, *press_node);
          text_param = text_node->create_parameter(ossia::val_type::STRING);
          press_node->add_child(std::move(text_node));
        }

        m_screen->onKey = [press_param, text_param](int key, const QString& text) {
          press_param->push_value(key);
          text_param->push_value(text.toStdString());
        };
        node->add_child(std::move(press_node));
      }
      {
        auto release_node
            = std::make_unique<ossia::net::generic_node>("release", *this, *node);
        ossia::net::parameter_base* press_param{};
        ossia::net::parameter_base* text_param{};
        {
          auto code_node
              = std::make_unique<ossia::net::generic_node>("code", *this, *release_node);
          press_param = code_node->create_parameter(ossia::val_type::INT);
          press_param->push_value(ossia::vec2f{0.f, 0.f});
          release_node->add_child(std::move(code_node));
        }
        {
          auto text_node
              = std::make_unique<ossia::net::generic_node>("text", *this, *release_node);
          text_param = text_node->create_parameter(ossia::val_type::STRING);
          release_node->add_child(std::move(text_node));
        }

        m_screen->onKeyRelease
            = [press_param, text_param](int key, const QString& text) {
          press_param->push_value(key);
          text_param->push_value(text.toStdString());
        };
        node->add_child(std::move(release_node));
      }

      m_root.add_child(std::move(node));
    }

    {
      auto fs_node
          = std::make_unique<ossia::net::generic_node>("fullscreen", *this, m_root);
      auto fs_param = fs_node->create_parameter(ossia::val_type::BOOL);
      fs_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<bool>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setFullScreen(v);
          });
        }
      });
      m_root.add_child(std::move(fs_node));
    }

    {
      auto fps_node = std::make_unique<ossia::net::generic_node>("fps", *this, m_root);
      auto fps_param = fps_node->create_parameter(ossia::val_type::FLOAT);
      m_screen->onFps = [fps_param](float fps) { fps_param->push_value(fps); };
      m_root.add_child(std::move(fps_node));
    }
  }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};

}
namespace Gfx
{
class DeviceBackgroundRenderer : public score::BackgroundRenderer
{
public:
  explicit DeviceBackgroundRenderer(score::gfx::BackgroundNode& node)
      : score::BackgroundRenderer{}
  {
    this->shared_readback = std::make_shared<QRhiReadbackResult>();
    node.shared_readback = this->shared_readback;
  }

  ~DeviceBackgroundRenderer() override { }

  bool render(QPainter* painter, const QRectF& rect) override
  {
    auto& m_readback = *shared_readback;
    const auto w = m_readback.pixelSize.width();
    const auto h = m_readback.pixelSize.height();
    painter->setRenderHint(
        QPainter::RenderHint::SmoothPixmapTransform,
        w > rect.width() && h > rect.height());
    int sz = w * h * 4;
    int bytes = m_readback.data.size();
    if(bytes > 0 && bytes >= sz)
    {
      QImage img{
          (const unsigned char*)m_readback.data.data(), w, h, w * 4,
          QImage::Format_RGBA8888};
      painter->drawImage(rect, img);
      return true;
    }
    return false;
  }

private:
  QPointer<Gfx::DocumentPlugin> plug;
  std::shared_ptr<QRhiReadbackResult> shared_readback;
};

class background_device : public ossia::net::device_base
{
  score::gfx::BackgroundNode* m_screen{};
  gfx_node_base m_root;
  QObject m_qtContext;
  QPointer<Scenario::ScenarioDocumentView> m_view;
  DeviceBackgroundRenderer* m_renderer{};

  ossia::net::parameter_base* scaled_win{};
  ossia::net::parameter_base* abs_win{};
  ossia::net::parameter_base* abs_tablet_win{};
  ossia::net::parameter_base* size_param{};
  ossia::net::parameter_base* rendersize_param{};
  ossia::net::parameter_base* press_param{};
  ossia::net::parameter_base* text_param{};

  boost::container::static_vector<QMetaObject::Connection, 8> con;

  void update_viewport()
  {
    auto v = rendersize_param->value();
    if(auto val = v.target<ossia::vec2f>())
    {
      auto dom = abs_win->get_domain();
      if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
      {
        ossia::set_max(dom, *val);
        abs_win->set_domain(std::move(dom));
        abs_tablet_win->set_domain(std::move(dom));
      }
      else
      {
        // Use the normal size
        v = size_param->value();
        if(auto val = v.target<ossia::vec2f>())
        {
          auto dom = abs_win->get_domain();
          if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
          {
            ossia::set_max(dom, *val);
            abs_win->set_domain(std::move(dom));
            abs_tablet_win->set_domain(std::move(dom));
          }
        }
      }
    }
    else
    {
      v = size_param->value();
      if(auto val = v.target<ossia::vec2f>())
      {
        auto dom = abs_win->get_domain();
        if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
        {
          ossia::set_max(dom, *val);
          abs_win->set_domain(std::move(dom));
          abs_tablet_win->set_domain(std::move(dom));
        }
      }
    }
  }

public:
  background_device(
      Scenario::ScenarioDocumentView& view, std::unique_ptr<gfx_protocol_base> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_screen{new score::gfx::BackgroundNode}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_screen, name}
      , m_view{&view}
  {
    this->m_capabilities.change_tree = true;
    m_renderer = new DeviceBackgroundRenderer{*m_screen};
    view.addBackgroundRenderer(m_renderer);

    auto& v = view.view();

    {
      auto size_node = std::make_unique<ossia::net::generic_node>("size", *this, m_root);
      size_param = size_node->create_parameter(ossia::val_type::VEC2F);
      const auto sz = view.view().size();
      m_screen->setSize(sz);
      size_param->push_value(ossia::vec2f{(float)sz.width(), (float)sz.height()});
      size_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setSize({(int)v[0], (int)v[1]});
          });

          update_viewport();
        }
      });

      con.push_back(
          QObject::connect(
              &v, &Scenario::ProcessGraphicsView::sizeChanged, &m_qtContext,
              [this, v = QPointer{&view}, ptr = QPointer{m_screen}](QSize e) {
        if(ptr && v)
        {
          size_param->push_value(ossia::vec2f{(float)e.width(), (float)e.height()});
        }
      }, Qt::DirectConnection));

      m_root.add_child(std::move(size_node));
    }
    {
      auto size_node
          = std::make_unique<ossia::net::generic_node>("rendersize", *this, m_root);

      rendersize_param = size_node->create_parameter(ossia::val_type::VEC2F);
      rendersize_param->push_value(ossia::vec2f{0.f, 0.f});
      rendersize_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = m_screen, v = *val] {
            screen->setRenderSize({(int)v[0], (int)v[1]});
          });

          update_viewport();
        }
      });

      m_root.add_child(std::move(size_node));
    }

    // Mouse input
    {
      auto node = std::make_unique<ossia::net::generic_node>("cursor", *this, m_root);
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("scaled", *this, *node);
        scaled_win = scale_node->create_parameter(ossia::val_type::VEC2F);
        scaled_win->set_domain(ossia::make_domain(0.f, 1.f));
        scaled_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(scale_node));
      }
      {
        auto abs_node
            = std::make_unique<ossia::net::generic_node>("absolute", *this, *node);
        abs_win = abs_node->create_parameter(ossia::val_type::VEC2F);
        abs_win->set_domain(
            ossia::make_domain(ossia::vec2f{0.f, 0.f}, ossia::vec2f{1280, 270.f}));
        abs_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(abs_node));
      }

      con.push_back(
          QObject::connect(
              &v, &Scenario::ProcessGraphicsView::hoverMove, &m_qtContext,
              [this, v = QPointer{&view}, ptr = QPointer{m_screen}](QHoverEvent* e) {
        if(ptr && v)
        {
          auto win = e->position();
          auto sz = v->view().size();
          scaled_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          abs_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
        }
      }, Qt::DirectConnection));

      m_root.add_child(std::move(node));
    }

    // Tablet input
    ossia::net::parameter_base* scaled_tablet_win{};
    {
      auto node = std::make_unique<ossia::net::generic_node>("tablet", *this, m_root);
      ossia::net::parameter_base* tablet_pressure{};
      ossia::net::parameter_base* tablet_z{};
      ossia::net::parameter_base* tablet_tan{};
      ossia::net::parameter_base* tablet_rot{};
      ossia::net::parameter_base* tablet_tilt_x{};
      ossia::net::parameter_base* tablet_tilt_y{};
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("scaled", *this, *node);
        scaled_tablet_win = scale_node->create_parameter(ossia::val_type::VEC2F);
        scaled_tablet_win->set_domain(ossia::make_domain(0.f, 1.f));
        scaled_tablet_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(scale_node));
      }
      {
        auto abs_node
            = std::make_unique<ossia::net::generic_node>("absolute", *this, *node);
        abs_tablet_win = abs_node->create_parameter(ossia::val_type::VEC2F);
        abs_tablet_win->set_domain(
            ossia::make_domain(ossia::vec2f{0.f, 0.f}, ossia::vec2f{1280, 270.f}));
        abs_tablet_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(abs_node));
      }
      {
        auto scale_node = std::make_unique<ossia::net::generic_node>("z", *this, *node);
        tablet_z = scale_node->create_parameter(ossia::val_type::INT);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("pressure", *this, *node);
        tablet_pressure = scale_node->create_parameter(ossia::val_type::FLOAT);
        //tablet_pressure->set_domain(ossia::make_domain(0.f, 1.f));
        //tablet_pressure->push_value(0.f);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tangential", *this, *node);
        tablet_tan = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tan->set_domain(ossia::make_domain(-1.f, 1.f));
        //tablet_tan->push_value(0.f);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("rotation", *this, *node);
        tablet_rot = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_rot->set_unit(ossia::degree_u{});
        tablet_rot->set_domain(ossia::make_domain(-180.f, 180.f));
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tilt_x", *this, *node);
        tablet_tilt_x = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tilt_x->set_domain(ossia::make_domain(-60.f, 60.f));
        tablet_tilt_x->set_unit(ossia::degree_u{});
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tilt_y", *this, *node);
        tablet_tilt_y = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tilt_y->set_domain(ossia::make_domain(-60.f, 60.f));
        tablet_tilt_y->set_unit(ossia::degree_u{});
        node->add_child(std::move(scale_node));
      }

      con.push_back(
          QObject::connect(
              &v, &Scenario::ProcessGraphicsView::tabletMove, m_screen,
              [=, this, v = QPointer{&view},
               ptr = QPointer{m_screen}](QTabletEvent* ev) {
        if(ptr && v)
        {
          auto win = ev->position();
          auto sz = v->view().size();
          scaled_tablet_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          abs_tablet_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
          tablet_pressure->push_value(ev->pressure());
          tablet_tan->push_value(ev->tangentialPressure());
          tablet_rot->push_value(ev->rotation());
          tablet_z->push_value(ev->z());
          tablet_tilt_x->push_value(ev->xTilt());
          tablet_tilt_y->push_value(ev->yTilt());
        }
      },
              Qt::DirectConnection));
      m_root.add_child(std::move(node));
    }

    // Keyboard input
    {
      auto node = std::make_unique<ossia::net::generic_node>("key", *this, m_root);
      {
        auto press_node
            = std::make_unique<ossia::net::generic_node>("press", *this, *node);
        {
          auto code_node
              = std::make_unique<ossia::net::generic_node>("code", *this, *press_node);
          press_param = code_node->create_parameter(ossia::val_type::INT);
          press_param->push_value(ossia::vec2f{0.f, 0.f});
          press_node->add_child(std::move(code_node));
        }
        {
          auto text_node
              = std::make_unique<ossia::net::generic_node>("text", *this, *press_node);
          text_param = text_node->create_parameter(ossia::val_type::STRING);
          press_node->add_child(std::move(text_node));
        }

        con.push_back(
            QObject::connect(
                &v, &Scenario::ProcessGraphicsView::keyPress, m_screen,
                [this, v = QPointer{&view}, ptr = QPointer{m_screen}](QKeyEvent* ev) {
          if(ptr && v)
          {
            if(!ev->isAutoRepeat())
            {
              press_param->push_value(ev->key());
              text_param->push_value(ev->text().toStdString());
            }
          }
        }, Qt::DirectConnection));
        node->add_child(std::move(press_node));
      }
      {
        auto release_node
            = std::make_unique<ossia::net::generic_node>("release", *this, *node);
        ossia::net::parameter_base* press_param{};
        ossia::net::parameter_base* text_param{};
        {
          auto code_node
              = std::make_unique<ossia::net::generic_node>("code", *this, *release_node);
          press_param = code_node->create_parameter(ossia::val_type::INT);
          press_param->push_value(ossia::vec2f{0.f, 0.f});
          release_node->add_child(std::move(code_node));
        }
        {
          auto text_node
              = std::make_unique<ossia::net::generic_node>("text", *this, *release_node);
          text_param = text_node->create_parameter(ossia::val_type::STRING);
          release_node->add_child(std::move(text_node));
        }

        con.push_back(
            QObject::connect(
                &v, &Scenario::ProcessGraphicsView::keyRelease, m_screen,
                [=, v = QPointer{&view}, ptr = QPointer{m_screen}](QKeyEvent* ev) {
          if(ptr && v)
          {
            if(!ev->isAutoRepeat())
            {
              press_param->push_value(ev->key());
              text_param->push_value(ev->text().toStdString());
            }
          }
        }, Qt::DirectConnection));
        node->add_child(std::move(release_node));
      }

      m_root.add_child(std::move(node));
    }
  }

  ~background_device()
  {
    if(m_view)
    {
      m_view->removeBackgroundRenderer(m_renderer);
    }
    for(auto& c : con)
      QObject::disconnect(c);
    delete m_renderer;
    m_protocol->stop();

    m_root.clear_children();

    m_protocol.reset();
  }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};

static score::gfx::MultiWindowNode* createMultiWindowNode(
    const std::vector<OutputMapping>& mappings)
{
  const auto& settings = score::AppContext().applicationSettings;
  const auto& gfx_settings = score::AppContext().settings<Gfx::Settings::Model>();

  score::gfx::OutputNode::Configuration conf;
  double rate = gfx_settings.getRate();
  if(rate > 0)
    conf = {.manualRenderingRate = 1000. / rate, .supportsVSync = false};
  else
    conf = {.manualRenderingRate = 1000. / 60., .supportsVSync = false};

  return new score::gfx::MultiWindowNode{conf, mappings};
}

class multiwindow_device : public ossia::net::device_base
{
  score::gfx::MultiWindowNode* m_node{};
  gfx_node_base m_root;
  QObject m_qtContext;

  ossia::net::parameter_base* fps_param{};
  ossia::net::parameter_base* rendersize_param{};

  struct PerWindowParams
  {
    ossia::net::parameter_base* scaled_cursor{};
    ossia::net::parameter_base* abs_cursor{};
    ossia::net::parameter_base* size_param{};
    ossia::net::parameter_base* pos_param{};
    ossia::net::parameter_base* source_pos{};
    ossia::net::parameter_base* source_size{};
    ossia::net::parameter_base* key_press_code{};
    ossia::net::parameter_base* key_press_text{};
    ossia::net::parameter_base* key_release_code{};
    ossia::net::parameter_base* key_release_text{};

    std::vector<QMetaObject::Connection> connections;
  };
  std::vector<PerWindowParams> m_perWindow;

  void update_viewport(PerWindowParams& pw)
  {
    if(!rendersize_param || !pw.abs_cursor)
      return;

    auto v = rendersize_param->value();
    if(auto val = v.target<ossia::vec2f>())
    {
      if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
      {
        auto dom = pw.abs_cursor->get_domain();
        ossia::set_max(dom, *val);
        pw.abs_cursor->set_domain(std::move(dom));
      }
      else if(pw.size_param)
      {
        v = pw.size_param->value();
        if(auto sz = v.target<ossia::vec2f>())
        {
          if((*sz)[0] >= 1.f && (*sz)[1] >= 1.f)
          {
            auto dom = pw.abs_cursor->get_domain();
            ossia::set_max(dom, *sz);
            pw.abs_cursor->set_domain(std::move(dom));
          }
        }
      }
    }
  }

public:
  multiwindow_device(
      const std::vector<OutputMapping>& mappings,
      std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_node{createMultiWindowNode(mappings)}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_node, name}
  {
    this->m_capabilities.change_tree = true;

    // Connect window signals once windows are created by createOutput()
    m_node->onWindowsCreated = [this] { connectWindowSignals(); };

    // Global FPS output parameter
    {
      auto fps_node = std::make_unique<ossia::net::generic_node>("fps", *this, m_root);
      fps_param = fps_node->create_parameter(ossia::val_type::FLOAT);
      m_node->onFps = [this](float fps) { fps_param->push_value(fps); };
      m_root.add_child(std::move(fps_node));
    }

    // Global rendersize parameter
    {
      auto rs_node
          = std::make_unique<ossia::net::generic_node>("rendersize", *this, m_root);
      ossia::net::set_description(*rs_node, "Set to [0, 0] to use the viewport's size");
      rendersize_param = rs_node->create_parameter(ossia::val_type::VEC2F);
      rendersize_param->push_value(ossia::vec2f{0.f, 0.f});
      rendersize_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          m_node->setRenderSize({(int)(*val)[0], (int)(*val)[1]});
          for(auto& pw : m_perWindow)
            update_viewport(pw);
        }
      });
      m_root.add_child(std::move(rs_node));
    }

    // Per-window subtrees: /0, /1, /2, ...
    m_perWindow.resize(mappings.size());

    for(int i = 0; i < (int)mappings.size(); ++i)
    {
      auto& pw = m_perWindow[i];
      const auto& mapping = mappings[i];
      const auto idx_str = std::to_string(i);

      auto win_node
          = std::make_unique<ossia::net::generic_node>(idx_str, *this, m_root);

      // /i/cursor
      {
        auto cursor_node
            = std::make_unique<ossia::net::generic_node>("cursor", *this, *win_node);
        {
          auto scale_node
              = std::make_unique<ossia::net::generic_node>("scaled", *this, *cursor_node);
          pw.scaled_cursor = scale_node->create_parameter(ossia::val_type::VEC2F);
          pw.scaled_cursor->set_domain(ossia::make_domain(0.f, 1.f));
          pw.scaled_cursor->push_value(ossia::vec2f{0.f, 0.f});
          cursor_node->add_child(std::move(scale_node));
        }
        {
          auto abs_node
              = std::make_unique<ossia::net::generic_node>("absolute", *this, *cursor_node);
          pw.abs_cursor = abs_node->create_parameter(ossia::val_type::VEC2F);
          pw.abs_cursor->set_domain(ossia::make_domain(
              ossia::vec2f{0.f, 0.f},
              ossia::vec2f{(float)mapping.windowSize.width(),
                           (float)mapping.windowSize.height()}));
          pw.abs_cursor->push_value(ossia::vec2f{0.f, 0.f});
          cursor_node->add_child(std::move(abs_node));
        }
        win_node->add_child(std::move(cursor_node));
      }

      // /i/size
      {
        auto size_node
            = std::make_unique<ossia::net::generic_node>("size", *this, *win_node);
        pw.size_param = size_node->create_parameter(ossia::val_type::VEC2F);
        pw.size_param->push_value(ossia::vec2f{
            (float)mapping.windowSize.width(), (float)mapping.windowSize.height()});
        pw.size_param->add_callback([this, i](const ossia::value& v) {
          if(auto val = v.target<ossia::vec2f>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, v = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                  w->resize(QSize{(int)v[0], (int)v[1]});
              }
            });
            if(i < (int)m_perWindow.size())
              update_viewport(m_perWindow[i]);
          }
        });
        win_node->add_child(std::move(size_node));
      }

      // /i/position
      {
        auto pos_node
            = std::make_unique<ossia::net::generic_node>("position", *this, *win_node);
        pw.pos_param = pos_node->create_parameter(ossia::val_type::VEC2F);
        pw.pos_param->push_value(ossia::vec2f{
            (float)mapping.windowPosition.x(), (float)mapping.windowPosition.y()});
        pw.pos_param->add_callback([this, i](const ossia::value& v) {
          if(auto val = v.target<ossia::vec2f>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, v = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                  w->setPosition(QPoint{(int)v[0], (int)v[1]});
              }
            });
          }
        });
        win_node->add_child(std::move(pos_node));
      }

      // /i/fullscreen
      {
        auto fs_node
            = std::make_unique<ossia::net::generic_node>("fullscreen", *this, *win_node);
        auto fs_param = fs_node->create_parameter(ossia::val_type::BOOL);
        fs_param->push_value(mapping.fullscreen);
        fs_param->add_callback([this, i](const ossia::value& v) {
          if(auto val = v.target<bool>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, v = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                {
                  if(v)
                    w->showFullScreen();
                  else
                    w->showNormal();
                }
              }
            });
          }
        });
        win_node->add_child(std::move(fs_node));
      }

      // /i/screen
      {
        auto screen_node
            = std::make_unique<ossia::net::generic_node>("screen", *this, *win_node);
        auto screen_param = screen_node->create_parameter(ossia::val_type::STRING);
        screen_param->set_domain(ossia::make_domain(int(0), int(100)));
        screen_param->add_callback([this, i](const ossia::value& v) {
          if(auto val = v.target<int>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, scr = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                {
                  const auto& cur_screens = qApp->screens();
                  if(ossia::valid_index(scr, cur_screens))
                    w->setScreen(cur_screens[scr]);
                }
              }
            });
          }
          else if(auto val = v.target<std::string>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, scr = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                {
                  const auto& cur_screens = qApp->screens();
                  for(auto s : cur_screens)
                    if(s->name() == scr.c_str())
                    {
                      w->setScreen(s);
                      break;
                    }
                }
              }
            });
          }
        });
        win_node->add_child(std::move(screen_node));
      }

      // /i/source (UV mapping rectangle)
      {
        auto source_node
            = std::make_unique<ossia::net::generic_node>("source", *this, *win_node);
        {
          auto pos_node
              = std::make_unique<ossia::net::generic_node>("position", *this, *source_node);
          ossia::net::set_description(*pos_node, "UV position of source rect (0-1)");
          pw.source_pos = pos_node->create_parameter(ossia::val_type::VEC2F);
          pw.source_pos->set_domain(ossia::make_domain(0.f, 1.f));
          pw.source_pos->push_value(ossia::vec2f{
              (float)mapping.sourceRect.x(), (float)mapping.sourceRect.y()});
          pw.source_pos->add_callback([this, i](const ossia::value& v) {
            if(auto val = v.target<ossia::vec2f>())
            {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                auto r = outputs[i].sourceRect;
                r.moveTopLeft(QPointF{(*val)[0], (*val)[1]});
                m_node->setSourceRect(i, r);
              }
            }
          });
          source_node->add_child(std::move(pos_node));
        }
        {
          auto sz_node
              = std::make_unique<ossia::net::generic_node>("size", *this, *source_node);
          ossia::net::set_description(*sz_node, "UV size of source rect (0-1)");
          pw.source_size = sz_node->create_parameter(ossia::val_type::VEC2F);
          pw.source_size->set_domain(ossia::make_domain(0.f, 1.f));
          pw.source_size->push_value(ossia::vec2f{
              (float)mapping.sourceRect.width(), (float)mapping.sourceRect.height()});
          pw.source_size->add_callback([this, i](const ossia::value& v) {
            if(auto val = v.target<ossia::vec2f>())
            {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                auto r = outputs[i].sourceRect;
                r.setSize(QSizeF{(*val)[0], (*val)[1]});
                m_node->setSourceRect(i, r);
              }
            }
          });
          source_node->add_child(std::move(sz_node));
        }
        win_node->add_child(std::move(source_node));
      }

      // /i/blend (soft-edge blending per side)
      {
        auto blend_node
            = std::make_unique<ossia::net::generic_node>("blend", *this, *win_node);

        struct BlendSide { const char* name; int side; float initW; float initG; };
        BlendSide sides[] = {
            {"left", 0, mapping.blendLeft.width, mapping.blendLeft.gamma},
            {"right", 1, mapping.blendRight.width, mapping.blendRight.gamma},
            {"top", 2, mapping.blendTop.width, mapping.blendTop.gamma},
            {"bottom", 3, mapping.blendBottom.width, mapping.blendBottom.gamma}};

        for(auto& [sname, side, initW, initG] : sides)
        {
          auto side_node
              = std::make_unique<ossia::net::generic_node>(sname, *this, *blend_node);
          {
            auto w_node
                = std::make_unique<ossia::net::generic_node>("width", *this, *side_node);
            ossia::net::set_description(*w_node, "Blend width in UV space (0-0.5)");
            auto w_param = w_node->create_parameter(ossia::val_type::FLOAT);
            w_param->set_domain(ossia::make_domain(0.f, 0.5f));
            w_param->push_value(initW);
            w_param->add_callback([this, i, side](const ossia::value& v) {
              if(auto val = v.target<float>())
              {
                const auto& outputs = m_node->windowOutputs();
                if(i < (int)outputs.size())
                {
                  float gamma = 2.2f;
                  switch(side) {
                    case 0: gamma = outputs[i].blendLeft.gamma; break;
                    case 1: gamma = outputs[i].blendRight.gamma; break;
                    case 2: gamma = outputs[i].blendTop.gamma; break;
                    case 3: gamma = outputs[i].blendBottom.gamma; break;
                  }
                  m_node->setEdgeBlend(i, side, *val, gamma);
                }
              }
            });
            side_node->add_child(std::move(w_node));
          }
          {
            auto g_node
                = std::make_unique<ossia::net::generic_node>("gamma", *this, *side_node);
            ossia::net::set_description(*g_node, "Blend curve exponent (0.1-4.0)");
            auto g_param = g_node->create_parameter(ossia::val_type::FLOAT);
            g_param->set_domain(ossia::make_domain(0.1f, 4.f));
            g_param->push_value(initG);
            g_param->add_callback([this, i, side](const ossia::value& v) {
              if(auto val = v.target<float>())
              {
                const auto& outputs = m_node->windowOutputs();
                if(i < (int)outputs.size())
                {
                  float width = 0.f;
                  switch(side) {
                    case 0: width = outputs[i].blendLeft.width; break;
                    case 1: width = outputs[i].blendRight.width; break;
                    case 2: width = outputs[i].blendTop.width; break;
                    case 3: width = outputs[i].blendBottom.width; break;
                  }
                  m_node->setEdgeBlend(i, side, width, *val);
                }
              }
            });
            side_node->add_child(std::move(g_node));
          }
          blend_node->add_child(std::move(side_node));
        }
        win_node->add_child(std::move(blend_node));
      }

      // /i/key
      {
        auto key_node
            = std::make_unique<ossia::net::generic_node>("key", *this, *win_node);

        // /i/key/press
        {
          auto press_node
              = std::make_unique<ossia::net::generic_node>("press", *this, *key_node);
          {
            auto code_node
                = std::make_unique<ossia::net::generic_node>("code", *this, *press_node);
            pw.key_press_code = code_node->create_parameter(ossia::val_type::INT);
            press_node->add_child(std::move(code_node));
          }
          {
            auto text_node
                = std::make_unique<ossia::net::generic_node>("text", *this, *press_node);
            pw.key_press_text = text_node->create_parameter(ossia::val_type::STRING);
            press_node->add_child(std::move(text_node));
          }
          key_node->add_child(std::move(press_node));
        }

        // /i/key/release
        {
          auto release_node
              = std::make_unique<ossia::net::generic_node>("release", *this, *key_node);
          {
            auto code_node
                = std::make_unique<ossia::net::generic_node>("code", *this, *release_node);
            pw.key_release_code = code_node->create_parameter(ossia::val_type::INT);
            release_node->add_child(std::move(code_node));
          }
          {
            auto text_node
                = std::make_unique<ossia::net::generic_node>("text", *this, *release_node);
            pw.key_release_text = text_node->create_parameter(ossia::val_type::STRING);
            release_node->add_child(std::move(text_node));
          }
          key_node->add_child(std::move(release_node));
        }

        win_node->add_child(std::move(key_node));
      }

      m_root.add_child(std::move(win_node));
    }
  }

  // Called after windows are created by createOutput() to connect Qt signals
  void connectWindowSignals()
  {
    const auto& outputs = m_node->windowOutputs();
    for(int i = 0; i < (int)outputs.size() && i < (int)m_perWindow.size(); ++i)
    {
      auto& pw = m_perWindow[i];
      const auto& wo = outputs[i];
      if(!wo.window)
        continue;

      auto* w = wo.window.get();

      // Mouse cursor
      pw.connections.push_back(
          QObject::connect(w, &score::gfx::Window::mouseMove,
              [&pw, w](QPointF screen, QPointF win) {
                auto sz = w->size();
                if(sz.width() > 0 && sz.height() > 0)
                {
                  pw.scaled_cursor->push_value(ossia::vec2f{
                      float(win.x() / sz.width()), float(win.y() / sz.height())});
                  pw.abs_cursor->push_value(
                      ossia::vec2f{float(win.x()), float(win.y())});
                }
              }));

      // Window position feedback
      pw.connections.push_back(
          QObject::connect(w, &QWindow::xChanged, [&pw, w](int x) {
            pw.pos_param->set_value(ossia::vec2f{float(x), float(w->y())});
          }));
      pw.connections.push_back(
          QObject::connect(w, &QWindow::yChanged, [&pw, w](int y) {
            pw.pos_param->set_value(ossia::vec2f{float(w->x()), float(y)});
          }));

      // Keyboard
      pw.connections.push_back(
          QObject::connect(w, &score::gfx::Window::key,
              [&pw](int k, const QString& t) {
                pw.key_press_code->push_value(k);
                pw.key_press_text->push_value(t.toStdString());
              }));
      pw.connections.push_back(
          QObject::connect(w, &score::gfx::Window::keyRelease,
              [&pw](int k, const QString& t) {
                pw.key_release_code->push_value(k);
                pw.key_release_text->push_value(t.toStdString());
              }));
    }
  }

  ~multiwindow_device()
  {
    // Disconnect all window signals
    for(auto& pw : m_perWindow)
      for(auto& c : pw.connections)
        QObject::disconnect(c);
    m_perWindow.clear();

    m_node->onWindowsCreated = [] {};
    m_node->onFps = [](float) {};
    m_protocol->stop();
    m_root.clear_children();
    m_protocol.reset();
  }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};

score::gfx::Window* WindowDevice::window() const noexcept
{
  if(m_dev)
  {
    auto p = m_dev.get()->get_root_node().get_parameter();
    if(auto param = safe_cast<gfx_parameter_base*>(p))
    {
      if(auto s = dynamic_cast<score::gfx::ScreenNode*>(param->node))
      {
        if(const auto& w = s->window())
        {
          return w.get();
        }
      }
    }
  }
  return nullptr;
}

WindowDevice::~WindowDevice() { }

void WindowDevice::addAddress(const Device::FullAddressSettings& settings)
{
  if(!m_dev)
    return;

  updateAddress(settings.address, settings);
}

void WindowDevice::setupContextMenu(QMenu& menu) const
{
  if(auto w = this->window())
  {
    auto showhide = new QAction;
    if(!w->isVisible())
    {
      showhide->setText(tr("Show"));
      connect(showhide, &QAction::triggered, w, &score::gfx::Window::show);
    }
    else
    {
      showhide->setText(tr("Hide"));
      connect(showhide, &QAction::triggered, w, &score::gfx::Window::hide);
    }
    menu.addAction(showhide);
  }
}

void WindowDevice::disconnect()
{
  DeviceInterface::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
}

bool WindowDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();

    if(plug)
    {
      m_protocol = new gfx_protocol_base{plug->exec};
      auto set = m_settings.deviceSpecificSettings.value<WindowSettings>();
      auto view = m_ctx.document.view();
      auto main_view = view ? qobject_cast<Scenario::ScenarioDocumentView*>(
          &view->viewDelegate()) : nullptr;
      switch(set.mode)
      {
        case WindowMode::Background: {
          if(main_view)
          {
            m_dev = std::make_unique<background_device>(
                *main_view, std::unique_ptr<gfx_protocol_base>(m_protocol),
                m_settings.name.toStdString());
          }
          else
          {
            m_dev = std::make_unique<window_device>(
                std::unique_ptr<gfx_protocol_base>(m_protocol),
                m_settings.name.toStdString());
          }
          break;
        }
        case WindowMode::MultiWindow: {
          m_dev = std::make_unique<multiwindow_device>(
              set.outputs, std::unique_ptr<gfx_protocol_base>(m_protocol),
              m_settings.name.toStdString());
          break;
        }
        case WindowMode::Single:
        default: {
          m_dev = std::make_unique<window_device>(
              std::unique_ptr<gfx_protocol_base>(m_protocol),
              m_settings.name.toStdString());
          break;
        }
      }

      enableCallbacks();
      deviceChanged(nullptr, m_dev.get());
    }
    // TODOengine->reload(&proto);

    // setLogging_impl(Device::get_cur_logging(isLogging()));
  }
  catch(std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch(...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

QString WindowProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Window");
}

QString WindowProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

QUrl WindowProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/window-device.html");
}

Device::DeviceInterface* WindowProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new WindowDevice{settings, ctx};
}

const Device::DeviceSettings& WindowProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Window";
    return s;
  }();
  return settings;
}

Device::AddressDialog* WindowProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* WindowProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* WindowProtocolFactory::makeSettingsWidget()
{
  return new WindowSettingsWidget;
}

QVariant
WindowProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<WindowSettings>(visitor);
}

void WindowProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<WindowSettings>(data, visitor);
}

bool WindowProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}

// --- OutputMappingItem implementation ---

OutputMappingItem::OutputMappingItem(int index, const QRectF& rect, QGraphicsItem* parent)
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

OutputMappingItem::BlendHandle OutputMappingItem::hitTestBlendHandles(const QPointF& pos) const
{
  constexpr double handleMargin = 5.0;
  // Minimum inset from the edge so the handle zone never overlaps with the
  // 6px resize margin. This ensures handles are grabbable even at width=0.
  constexpr double minInset = 10.0;
  auto r = rect();

  {
    double handleX = r.left() + std::max((double)blendLeft.width * r.width(), minInset);
    if(std::abs(pos.x() - handleX) < handleMargin
       && pos.y() >= r.top() && pos.y() <= r.bottom())
      return BlendLeft;
  }
  {
    double handleX = r.right() - std::max((double)blendRight.width * r.width(), minInset);
    if(std::abs(pos.x() - handleX) < handleMargin
       && pos.y() >= r.top() && pos.y() <= r.bottom())
      return BlendRight;
  }
  {
    double handleY = r.top() + std::max((double)blendTop.width * r.height(), minInset);
    if(std::abs(pos.y() - handleY) < handleMargin
       && pos.x() >= r.left() && pos.x() <= r.right())
      return BlendTop;
  }
  {
    double handleY = r.bottom() - std::max((double)blendBottom.width * r.height(), minInset);
    if(std::abs(pos.y() - handleY) < handleMargin
       && pos.x() >= r.left() && pos.x() <= r.right())
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
  const QColor handleColors[4] = {
      QColor(255, 80, 80), QColor(80, 255, 80),
      QColor(80, 80, 255), QColor(255, 255, 80)};

  for(int i = 0; i < 4; i++)
  {
    m_handles[i] = m_scene.addEllipse(
        -handleR, -handleR, handleR * 2, handleR * 2,
        QPen(Qt::white, 1), QBrush(handleColors[i]));
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

  const QPointF corners[4] = {
      toScene(m_warp.topLeft), toScene(m_warp.topRight),
      toScene(m_warp.bottomLeft), toScene(m_warp.bottomRight)};

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
      auto* line = m_scene.addLine(
          QLineF(bilinear(u0, v), bilinear(u1, v)), gridPen);
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
      auto* line = m_scene.addLine(
          QLineF(bilinear(u, v0), bilinear(u, v1)), gridPen);
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

    QPointF* corners[4] = {
        &m_warp.topLeft, &m_warp.topRight,
        &m_warp.bottomLeft, &m_warp.bottomRight};
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

// --- Test card rendering ---

static QImage renderTestCard(int w, int h)
{
  if(w < 1 || h < 1)
    return {};

  QImage img(w, h, QImage::Format_RGB32);
  img.fill(Qt::black);
  QPainter p(&img);

  const double cx = w * 0.5;
  const double cy = h * 0.5;
  const int gridStep = qMax(20, qMin(w, h) / 20);
  const double dim = qMin(w, h);

  // Layer 1: Checkerboard background
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    QColor light(60, 60, 60);
    QColor dark(30, 30, 30);
    for(int y = 0; y < h; y += gridStep)
      for(int x = 0; x < w; x += gridStep)
      {
        bool even = ((x / gridStep) + (y / gridStep)) % 2 == 0;
        p.fillRect(x, y, gridStep, gridStep, even ? dark : light);
      }
  }

  // Layer 2: Gradient bars — symmetric, centered
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    // Horizontal gradient bar (centered, 60% width, at ~70% height)
    int barH = qMax(8, (int)(h * 0.03));
    int barW = (int)(w * 0.6);
    int barX = (w - barW) / 2;
    int barY = (int)(h * 0.70);
    // Black-to-white-to-black
    for(int x = 0; x < barW; ++x)
    {
      double t = (double)x / qMax(1, barW - 1); // 0..1
      double v = t < 0.5 ? t * 2.0 : (1.0 - t) * 2.0;
      int gray = (int)(v * 255);
      p.setPen(QColor(gray, gray, gray));
      p.drawLine(barX + x, barY, barX + x, barY + barH - 1);
    }

    // Vertical gradient bar (centered, 60% height, at ~70% width from left, mirrored at 30%)
    int vBarW = qMax(8, (int)(w * 0.02));
    int vBarH = (int)(h * 0.6);
    int vBarY = (h - vBarH) / 2;
    int vBarX1 = (int)(w * 0.30) - vBarW / 2;
    int vBarX2 = (int)(w * 0.70) - vBarW / 2;
    for(int y = 0; y < vBarH; ++y)
    {
      double t = (double)y / qMax(1, vBarH - 1);
      double v = t < 0.5 ? t * 2.0 : (1.0 - t) * 2.0;
      int gray = (int)(v * 255);
      p.setPen(QColor(gray, gray, gray));
      p.drawLine(vBarX1, vBarY + y, vBarX1 + vBarW - 1, vBarY + y);
      p.drawLine(vBarX2, vBarY + y, vBarX2 + vBarW - 1, vBarY + y);
    }
  }

  // Layer 3: Color bars — rainbow strip centered horizontally, below center
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    const QColor colors[] = {
        QColor(255, 0, 0),   QColor(255, 128, 0), QColor(255, 255, 0),
        QColor(128, 255, 0), QColor(0, 255, 0),   QColor(0, 255, 128),
        QColor(0, 255, 255), QColor(0, 128, 255), QColor(0, 0, 255),
        QColor(128, 0, 255), QColor(255, 0, 255), QColor(255, 0, 128)};
    int nColors = 12;
    int stripW = (int)(w * 0.5);
    int stripH = qMax(8, (int)(h * 0.04));
    int stripX = (w - stripW) / 2;
    int stripY = (int)(h * 0.62);
    int cellW = stripW / nColors;
    for(int i = 0; i < nColors; i++)
    {
      int x0 = stripX + i * cellW;
      int x1 = (i == nColors - 1) ? stripX + stripW : stripX + (i + 1) * cellW;
      p.fillRect(x0, stripY, x1 - x0, stripH, colors[i]);
    }

    // Vertical color bars on left and right sides (mirrored)
    int vStripH = (int)(h * 0.5);
    int vStripW = qMax(8, (int)(w * 0.025));
    int vStripY = (h - vStripH) / 2;
    int vStripXL = (int)(w * 0.10);
    int vStripXR = (int)(w * 0.90) - vStripW;
    int cellH = vStripH / nColors;
    for(int i = 0; i < nColors; i++)
    {
      int y0 = vStripY + i * cellH;
      int y1 = (i == nColors - 1) ? vStripY + vStripH : vStripY + (i + 1) * cellH;
      p.fillRect(vStripXL, y0, vStripW, y1 - y0, colors[i]);
      p.fillRect(vStripXR, y0, vStripW, y1 - y0, colors[i]);
    }
  }

  p.setRenderHint(QPainter::Antialiasing, true);

  // Layer 4: Circles centered on the image
  {
    p.setBrush(Qt::NoBrush);
    QPen circlePen(QColor(200, 200, 200, 120), qMax(1.0, dim * 0.002));
    p.setPen(circlePen);
    // Large circle (80% of min dimension)
    double r1 = dim * 0.40;
    p.drawEllipse(QPointF(cx, cy), r1, r1);
    // Medium circle (50%)
    double r2 = dim * 0.25;
    p.drawEllipse(QPointF(cx, cy), r2, r2);
    // Small circle (20%)
    double r3 = dim * 0.10;
    p.drawEllipse(QPointF(cx, cy), r3, r3);
  }

  // Layer 5: Diagonals (corner to corner) + center cross
  {
    QPen diagPen(QColor(120, 120, 120, 100), qMax(1.0, dim * 0.001));
    p.setPen(diagPen);
    p.drawLine(0, 0, w - 1, h - 1);
    p.drawLine(w - 1, 0, 0, h - 1);

    // Dashed center cross
    QPen crossPen(QColor(200, 200, 200, 160), qMax(1.0, dim * 0.001));
    crossPen.setStyle(Qt::DashLine);
    p.setPen(crossPen);
    p.drawLine(0, h / 2, w - 1, h / 2);
    p.drawLine(w / 2, 0, w / 2, h - 1);
  }

  // Layer 6: Grid lines
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen gridPen(QColor(100, 100, 100, 80), 1);
    p.setPen(gridPen);
    for(int x = 0; x <= w; x += gridStep)
      p.drawLine(x, 0, x, h - 1);
    for(int y = 0; y <= h; y += gridStep)
      p.drawLine(0, y, w - 1, y);
  }

  // Layer 7: Crosshairs every 200px
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen tickPen(QColor(180, 180, 180), 1);
    p.setPen(tickPen);
    int tickLen = qMax(4, (int)(dim * 0.008));
    for(int y = 0; y < h; y += gridStep * 5)
      for(int x = 0; x < w; x += gridStep * 5)
      {
        p.drawLine(x - tickLen, y, x + tickLen, y);
        p.drawLine(x, y - tickLen, x, y + tickLen);
      }
  }

  p.setRenderHint(QPainter::Antialiasing, true);

  // Layer 8: "ossia score" branding — above center, balanced around center line
  {
    QFont f(QStringLiteral("Ubuntu"), 1);
    f.setPixelSize(qMax(16, (int)(dim * 0.06)));
    f.setWeight(QFont::Bold);
    p.setFont(f);
    p.setPen(QColor(0x03, 0xc3, 0xdd));

    QFontMetrics fm(f);
    double gap = fm.horizontalAdvance(QChar(' ')) * 0.6;
    double wOssia = fm.horizontalAdvance(QStringLiteral("ossia"));
    double wScore = fm.horizontalAdvance(QStringLiteral("score"));
    int textY = (int)(cy - dim * 0.40);
    int textH = (int)(dim * 0.10);

    // Draw "ossia" to the left of center, "score" to the right
    double xOssia = cx - gap * 0.5 - wOssia;
    double xScore = cx + gap * 0.5;
    QRectF rOssia(xOssia, textY, wOssia, textH);
    QRectF rScore(xScore, textY, wScore, textH);
    p.drawText(rOssia, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("ossia"));
    p.drawText(rScore, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("score"));
  }

  // Layer 9: Resolution label — below center
  {
    QFont f(QStringLiteral("Ubuntu"), 1);
    f.setPixelSize(qMax(10, (int)(dim * 0.025)));
    p.setFont(f);
    p.setPen(QColor(200, 200, 200));
    QString label = QStringLiteral("%1 \u00d7 %2").arg(w).arg(h);
    QRect textRect(0, (int)(cy - dim * 0.31), w, (int)(dim * 0.06));
    p.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, label);
  }

  return img;
}

// --- PreviewWidget implementation ---

static QColor previewColorForIndex(int index)
{
  static constexpr QColor colors[] = {
      QColor(30, 60, 180),  QColor(180, 30, 30),  QColor(30, 150, 30),
      QColor(180, 150, 30), QColor(150, 30, 150),  QColor(30, 150, 150),
      QColor(200, 100, 30), QColor(100, 30, 200),
  };
  return colors[index % 8];
}

PreviewWidget::PreviewWidget(int index, PreviewContent content, QWidget* parent)
    : QWidget{parent, Qt::Window | Qt::WindowStaysOnTopHint}
    , m_index{index}
    , m_content{content}
{
  setWindowTitle(QStringLiteral("Output %1").arg(index));
}

void PreviewWidget::setOutputIndex(int idx)
{
  m_index = idx;
  setWindowTitle(QStringLiteral("Output %1").arg(idx));
  update();
}

void PreviewWidget::setPreviewContent(PreviewContent mode)
{
  m_content = mode;
  update();
}

void PreviewWidget::setOutputResolution(QSize sz)
{
  m_resolution = sz;
  update();
}

void PreviewWidget::setBlend(EdgeBlend left, EdgeBlend right, EdgeBlend top, EdgeBlend bottom)
{
  m_blendLeft = left;
  m_blendRight = right;
  m_blendTop = top;
  m_blendBottom = bottom;
  update();
}

void PreviewWidget::setSourceRect(QRectF rect)
{
  m_sourceRect = rect;
  update();
}

void PreviewWidget::setCornerWarp(const CornerWarp& warp)
{
  m_cornerWarp = warp;
  update();
}

void PreviewWidget::setGlobalTestCard(const QImage& img)
{
  m_globalTestCard = img;
  update();
}

// Build gradient stops approximating pow(1-t, gamma) opacity curve
static void setGammaGradientStops(QLinearGradient& grad, float gamma)
{
  constexpr int steps = 16;
  constexpr int maxAlpha = 180;
  for(int i = 0; i <= steps; i++)
  {
    double t = (double)i / steps;           // 0 = edge, 1 = interior
    double alpha = std::pow(1.0 - t, gamma); // 1 at edge, 0 at interior
    grad.setColorAt(t, QColor(0, 0, 0, (int)(alpha * maxAlpha)));
  }
}

static void paintBlendGradients(
    QPainter& p, const QRect& r, const EdgeBlend& left, const EdgeBlend& right,
    const EdgeBlend& top, const EdgeBlend& bottom)
{
  p.setPen(Qt::NoPen);

  if(left.width > 0.f)
  {
    double w = left.width * r.width();
    QLinearGradient grad(r.left(), r.center().y(), r.left() + w, r.center().y());
    setGammaGradientStops(grad, left.gamma);
    p.setBrush(QBrush(grad));
    p.drawRect(QRectF(r.left(), r.top(), w, r.height()));
  }

  if(right.width > 0.f)
  {
    double w = right.width * r.width();
    QLinearGradient grad(r.right(), r.center().y(), r.right() - w, r.center().y());
    setGammaGradientStops(grad, right.gamma);
    p.setBrush(QBrush(grad));
    p.drawRect(QRectF(r.right() - w, r.top(), w, r.height()));
  }

  if(top.width > 0.f)
  {
    double h = top.width * r.height();
    QLinearGradient grad(r.center().x(), r.top(), r.center().x(), r.top() + h);
    setGammaGradientStops(grad, top.gamma);
    p.setBrush(QBrush(grad));
    p.drawRect(QRectF(r.left(), r.top(), r.width(), h));
  }

  if(bottom.width > 0.f)
  {
    double h = bottom.width * r.height();
    QLinearGradient grad(r.center().x(), r.bottom(), r.center().x(), r.bottom() - h);
    setGammaGradientStops(grad, bottom.gamma);
    p.setBrush(QBrush(grad));
    p.drawRect(QRectF(r.left(), r.bottom() - h, r.width(), h));
  }
}

void PreviewWidget::paintEvent(QPaintEvent*)
{
  QPainter p(this);
  p.fillRect(rect(), Qt::black);

  // Apply corner warp transform if non-identity
  if(!m_cornerWarp.isIdentity())
  {
    double w = width(), h = height();
    QPolygonF src;
    src << QPointF(0, 0) << QPointF(w, 0) << QPointF(w, h) << QPointF(0, h);
    QPolygonF dst;
    dst << QPointF(m_cornerWarp.topLeft.x() * w, m_cornerWarp.topLeft.y() * h)
        << QPointF(m_cornerWarp.topRight.x() * w, m_cornerWarp.topRight.y() * h)
        << QPointF(m_cornerWarp.bottomRight.x() * w, m_cornerWarp.bottomRight.y() * h)
        << QPointF(m_cornerWarp.bottomLeft.x() * w, m_cornerWarp.bottomLeft.y() * h);

    QTransform t;
    if(QTransform::quadToQuad(src, dst, t))
      p.setTransform(t);
  }

  switch(m_content)
  {
    case PreviewContent::Black:
      p.fillRect(rect(), Qt::black);
      return;

    case PreviewContent::PerOutputTestCard: {
      QImage card = renderTestCard(m_resolution.width(), m_resolution.height());
      p.drawImage(rect(), card);
      paintBlendGradients(p, rect(), m_blendLeft, m_blendRight, m_blendTop, m_blendBottom);
      return;
    }

    case PreviewContent::GlobalTestCard: {
      if(!m_globalTestCard.isNull())
      {
        QRectF srcPixels(
            m_sourceRect.x() * m_globalTestCard.width(),
            m_sourceRect.y() * m_globalTestCard.height(),
            m_sourceRect.width() * m_globalTestCard.width(),
            m_sourceRect.height() * m_globalTestCard.height());
        p.drawImage(QRectF(rect()), m_globalTestCard, srcPixels);
      }
      else
      {
        p.fillRect(rect(), Qt::black);
      }
      paintBlendGradients(p, rect(), m_blendLeft, m_blendRight, m_blendTop, m_blendBottom);
      return;
    }

    case PreviewContent::OutputIdentification: {
      p.fillRect(rect(), previewColorForIndex(m_index));
      paintBlendGradients(p, rect(), m_blendLeft, m_blendRight, m_blendTop, m_blendBottom);

      p.setPen(Qt::white);

      {
        QFont f = p.font();
        f.setPixelSize(qMax(32, qMin(width(), height()) / 3));
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QString::number(m_index));
      }

      {
        QFont f = p.font();
        f.setPixelSize(qMax(14, qMin(width(), height()) / 12));
        f.setBold(false);
        p.setFont(f);
        auto text = QStringLiteral("%1 x %2").arg(m_resolution.width()).arg(m_resolution.height());
        QRect lower = rect();
        lower.setTop(rect().center().y() + qMin(width(), height()) / 5);
        p.drawText(lower, Qt::AlignHCenter | Qt::AlignTop, text);
      }
      return;
    }
  }
}

// --- OutputPreviewWindows implementation ---

OutputPreviewWindows::OutputPreviewWindows(QObject* parent)
    : QObject{parent}
{
}

OutputPreviewWindows::~OutputPreviewWindows()
{
  closeAll();
}

void OutputPreviewWindows::closeAll()
{
  qDeleteAll(m_windows);
  m_windows.clear();
}

void OutputPreviewWindows::syncToMappings(const std::vector<OutputMapping>& mappings)
{
  // Ensure global test card exists
  if(m_globalTestCard.isNull())
    rebuildGlobalTestCard();

  const int newCount = (int)mappings.size();
  const int oldCount = (int)m_windows.size();

  // Remove excess windows
  while((int)m_windows.size() > newCount)
  {
    delete m_windows.back();
    m_windows.pop_back();
  }

  // Add new windows
  for(int i = oldCount; i < newCount; ++i)
  {
    auto* pw = new PreviewWidget(i, m_content);
    m_windows.push_back(pw);
  }

  // Update positions/sizes/screens for all windows
  const auto& screens = qApp->screens();
  for(int i = 0; i < newCount; ++i)
  {
    auto* pw = m_windows[i];
    const auto& m = mappings[i];

    pw->setOutputResolution(m.windowSize);
    pw->setSourceRect(m.sourceRect);
    pw->setBlend(m.blendLeft, m.blendRight, m.blendTop, m.blendBottom);
    pw->setCornerWarp(m.cornerWarp);
    if(!m_globalTestCard.isNull())
      pw->setGlobalTestCard(m_globalTestCard);

    if(m_syncPositions)
    {
      if(m.fullscreen)
      {
        pw->showFullScreen();
      }
      else
      {
        if(pw->isFullScreen())
          pw->showNormal();

        pw->move(m.windowPosition);
        pw->resize(m.windowSize);
      }

      // Set screen via underlying QWindow (must be done after show)
      if(m.screenIndex >= 0 && m.screenIndex < screens.size())
      {
        if(auto* wh = pw->windowHandle())
          wh->setScreen(screens[m.screenIndex]);
      }
    }

    pw->show();
    pw->raise();
  }
}

void OutputPreviewWindows::setSyncPositions(bool sync)
{
  m_syncPositions = sync;
}

void OutputPreviewWindows::setPreviewContent(PreviewContent mode)
{
  m_content = mode;
  for(auto* pw : m_windows)
    pw->setPreviewContent(mode);
}

void OutputPreviewWindows::setInputResolution(QSize sz)
{
  if(m_inputResolution != sz)
  {
    m_inputResolution = sz;
    rebuildGlobalTestCard();
  }
}

void OutputPreviewWindows::rebuildGlobalTestCard()
{
  m_globalTestCard = renderTestCard(m_inputResolution.width(), m_inputResolution.height());
  for(auto* pw : m_windows)
    pw->setGlobalTestCard(m_globalTestCard);
}

// --- WindowSettingsWidget implementation ---

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
    m_syncPositionsCheck->setToolTip(tr("Synchronize preview window positions and sizes with the output mappings"));
    btnLayout->addWidget(m_syncPositionsCheck);

    connect(m_previewContentCombo, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](int idx) {
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

    auto makeBlendRow = [&](const QString& label, QDoubleSpinBox*& widthSpin, QDoubleSpinBox*& gammaSpin) {
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
    connect(m_screenCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, applyProps);
    connect(m_winPosX, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winPosY, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winWidth, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winHeight, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_fullscreenCheck, &QCheckBox::toggled, this, applyProps);
    connect(m_blendLeftW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(m_blendLeftG, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(m_blendRightW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(m_blendRightG, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(m_blendTopW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(m_blendTopG, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(m_blendBottomW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(m_blendBottomG, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);

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
    connect(m_screenCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, updatePx);

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
        m_screenCombo->setCurrentIndex(mi->screenIndex + 1); // +1 because index 0 is "Default"
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
        mi->screenIndex = m_screenCombo->currentIndex() - 1; // -1 because index 0 is "Default"
        mi->windowPosition = QPoint(m_winPosX->value(), m_winPosY->value());
        mi->windowSize = QSize(m_winWidth->value(), m_winHeight->value());
        mi->fullscreen = m_fullscreenCheck->isChecked();

        mi->blendLeft = {(float)m_blendLeftW->value(), (float)m_blendLeftG->value()};
        mi->blendRight = {(float)m_blendRightW->value(), (float)m_blendRightG->value()};
        mi->blendTop = {(float)m_blendTopW->value(), (float)m_blendTopG->value()};
        mi->blendBottom = {(float)m_blendBottomW->value(), (float)m_blendBottomG->value()};
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

template <>
void DataStreamReader::read(const Gfx::OutputMapping& n)
{
  m_stream << n.sourceRect << n.screenIndex << n.windowPosition << n.windowSize
           << n.fullscreen;
  m_stream << n.blendLeft.width << n.blendLeft.gamma;
  m_stream << n.blendRight.width << n.blendRight.gamma;
  m_stream << n.blendTop.width << n.blendTop.gamma;
  m_stream << n.blendBottom.width << n.blendBottom.gamma;
  m_stream << n.cornerWarp.topLeft << n.cornerWarp.topRight
           << n.cornerWarp.bottomLeft << n.cornerWarp.bottomRight;
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
  m_stream >> n.cornerWarp.topLeft >> n.cornerWarp.topRight
      >> n.cornerWarp.bottomLeft >> n.cornerWarp.bottomRight;
}

template <>
void DataStreamReader::read(const Gfx::WindowSettings& n)
{
  m_stream << (int32_t)5; // version tag
  m_stream << (int32_t)n.mode;
  m_stream << (int32_t)n.outputs.size();
  for(const auto& o : n.outputs)
    read(o);
  m_stream << (int32_t)n.inputWidth << (int32_t)n.inputHeight;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::WindowSettings& n)
{
  int32_t version{};
  m_stream >> version;
  if(version == 0 || version == 1)
  {
    // Old format: version is actually the bool background value
    n.mode = version ? Gfx::WindowMode::Background : Gfx::WindowMode::Single;
  }
  else if(version == 2)
  {
    int32_t mode{};
    m_stream >> mode;
    n.mode = (Gfx::WindowMode)mode;
    int32_t count{};
    m_stream >> count;
    n.outputs.resize(count);
    for(auto& o : n.outputs)
    {
      // Version 2: no blend data
      m_stream >> o.sourceRect >> o.screenIndex >> o.windowPosition >> o.windowSize
          >> o.fullscreen;
    }
  }
  else if(version == 3)
  {
    int32_t mode{};
    m_stream >> mode;
    n.mode = (Gfx::WindowMode)mode;
    int32_t count{};
    m_stream >> count;
    n.outputs.resize(count);
    for(auto& o : n.outputs)
    {
      // Version 3: blend data but no corner warp
      m_stream >> o.sourceRect >> o.screenIndex >> o.windowPosition >> o.windowSize
          >> o.fullscreen;
      m_stream >> o.blendLeft.width >> o.blendLeft.gamma;
      m_stream >> o.blendRight.width >> o.blendRight.gamma;
      m_stream >> o.blendTop.width >> o.blendTop.gamma;
      m_stream >> o.blendBottom.width >> o.blendBottom.gamma;
    }
  }
  else if(version == 4)
  {
    int32_t mode{};
    m_stream >> mode;
    n.mode = (Gfx::WindowMode)mode;
    int32_t count{};
    m_stream >> count;
    n.outputs.resize(count);
    for(auto& o : n.outputs)
      write(o);
  }
  else if(version == 5)
  {
    int32_t mode{};
    m_stream >> mode;
    n.mode = (Gfx::WindowMode)mode;
    int32_t count{};
    m_stream >> count;
    n.outputs.resize(count);
    for(auto& o : n.outputs)
      write(o);
    int32_t inW{}, inH{};
    m_stream >> inW >> inH;
    n.inputWidth = inW;
    n.inputHeight = inH;
  }
  checkDelimiter();
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

template <>
void JSONReader::read(const Gfx::WindowSettings& n)
{
  obj["Mode"] = (int)n.mode;
  obj["Outputs"] = n.outputs;
  obj["InputWidth"] = n.inputWidth;
  obj["InputHeight"] = n.inputHeight;
}

template <>
void JSONWriter::write(Gfx::WindowSettings& n)
{
  // Backward compatibility with old format
  if(auto v = obj.tryGet("Background"))
  {
    n.mode = v->toBool() ? Gfx::WindowMode::Background : Gfx::WindowMode::Single;
    return;
  }

  if(auto v = obj.tryGet("Mode"))
    n.mode = (Gfx::WindowMode)v->toInt();
  if(auto v = obj.tryGet("Outputs"))
  {
    const auto arr = v->toArray();
    n.outputs.resize(arr.Size());
    for(int i = 0; i < arr.Size(); i++)
    {
      JSONWriter w{arr[i]};
      w.write(n.outputs[i]);
    }
  }
  if(auto v = obj.tryGet("InputWidth"))
    n.inputWidth = v->toInt();
  if(auto v = obj.tryGet("InputHeight"))
    n.inputHeight = v->toInt();
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::WindowSettings);
