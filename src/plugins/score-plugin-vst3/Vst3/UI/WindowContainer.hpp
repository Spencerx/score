#pragma once
#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QWindow>

#include <pluginterfaces/gui/iplugview.h>

namespace vst3
{
class PlugFrame;

inline const char* currentPlatform()
{
#if defined(__APPLE__)
  return Steinberg::kPlatformTypeNSView;
#elif defined(_WIN32)
  return Steinberg::kPlatformTypeHWND;
#elif (!(defined(__APPLE__) || defined(_WIN32))) && __has_include(<xcb/xcb.h>)
  return Steinberg::kPlatformTypeX11EmbedWindowID;
#endif
  return "";
}

struct WindowContainer
{
  WId nativeId;
  QWindow* qwindow{};
  QWidget* container{};
  vst3::PlugFrame* frame{};

  double qtScaleFactor{1.};

  WindowContainer()
  {
    double r = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble();
    if(r > 0.1)
    {
      qtScaleFactor = 1. / r;
    }
  }

  auto setSizeFromQt(
      Steinberg::IPlugView& view, const Steinberg::ViewRect& r,
      QDialog& parentWindow) const
  {
    int w = r.getWidth();
    int h = r.getHeight();
    int qw = r.getWidth() * qtScaleFactor;
    int qh = r.getHeight() * qtScaleFactor;

    if(w == 0 || h == 0)
    {
      return std::make_pair(w, h);
    }

    if(view.canResize() == Steinberg::kResultTrue)
    {
      parentWindow.resize(QSize{qw, qh});
    }
    else
    {
      parentWindow.setFixedSize(QSize{qw, qh});
    }
    if(qwindow)
    {
      qwindow->resize(qw, qh);
    }
    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(qw, qh);
    }

    return std::make_pair(w, h);
  }

  void
  setSizeFromUser(Steinberg::IPlugView& view, const QSize& sz, QDialog& parentWindow)
  {
    if(view.canResize() != Steinberg::kResultTrue)
      return;
    return;

    int qw = sz.width();
    int qh = sz.height();
    Steinberg::ViewRect r;
    r.top = 0;
    r.left = 0;
    r.right = sz.width() / qtScaleFactor;
    r.bottom = sz.height() / qtScaleFactor;
    view.checkSizeConstraint(&r);

    parentWindow.resize(QSize(qw, qh));

    if(qwindow)
    {
      qwindow->resize(qw, qh);
    }

    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(qw, qh);
    }

    view.onSize(&r);
  }

  auto setSizeFromVst(
      Steinberg::IPlugView& view, Steinberg::ViewRect& r, QDialog& parentWindow)
  {
    int w = r.getWidth();
    int h = r.getHeight();
    int qw = r.getWidth() * qtScaleFactor;
    int qh = r.getHeight() * qtScaleFactor;

    if(w == 0 || h == 0)
    {
      view.onSize(&r);
      return std::make_pair(qw, qh);
    }

    if(view.canResize() == Steinberg::kResultTrue)
    {
      parentWindow.resize(QSize{qw, qh});
    }
    else
    {
      parentWindow.setFixedSize(QSize{qw, qh});
    }

    if(qwindow)
    {
      qwindow->resize(qw, qh);
    }
    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(qw, qh);
    }

    view.onSize(&r);

    return std::make_pair(qw, qh);
  }
};

class Window;
}
