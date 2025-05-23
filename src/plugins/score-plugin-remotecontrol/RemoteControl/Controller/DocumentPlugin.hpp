#pragma once

#include <Process/Process.hpp>

#include <JS/Qml/EditContext.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/detail/flat_map.hpp>

#include <RemoteControl/RemoteControlProvider.hpp>

#include <span>

namespace Engine
{
class ApplicationPlugin;
}
namespace RemoteControl::Controller
{
// In JS:
// connect to DocumentPlugin actions and map them to the JS actions.

// Multi-controllers : we want to map multiple fader pages
// DocumentPlugin combines all the inputs from all the controllers.

// Lock device & process to controller
class RemoteControlImpl;
class RemoteControlProvider;
class DocumentPlugin : public score::DocumentPlugin
{
public:
  using ControllerHandle = Process::RemoteControlInterface::ControllerHandle;
  DocumentPlugin(const score::DocumentContext& doc, QObject* parent);
  ~DocumentPlugin();

  std::shared_ptr<Process::RemoteControlInterface> acquireRemoteControlInterface();
  void releaseRemoteControlInterface(std::shared_ptr<Process::RemoteControlInterface>);

  std::vector<ControllerHandle> registerControllerGroup(
      RemoteControlImpl&, Process::RemoteControlInterface::ControllerHint hint,
      int count);

  void prevBank(RemoteControlImpl&);
  void nextBank(RemoteControlImpl&);
  void prevChannel(RemoteControlImpl&);
  void nextChannel(RemoteControlImpl&);
  void setControl(RemoteControlImpl&, ControllerHandle index, const ossia::value& val);
  void offsetControl(RemoteControlImpl&, ControllerHandle index, double val);

  JS::EditJsContext editContext;

private:
  void connectToEngine();
  void disconnectFromEngine();
  void reloadProcess();

  void on_execTime();
  void on_selectionChanged(const Selection& old, const Selection& current);

  void deselectProcess();
  void selectProcess(Process::ProcessModel& cur);

  void on_controlChanged(
      Process::ProcessModel& p, Process::ControlInlet& inl, int idx,
      const ossia::value& v);

  void clearDisplay();
  void updateDisplayedControls();
  void updateDisplay();

  Engine::ApplicationPlugin* m_engine{};
  Process::ProcessModel* m_currentProcess{};
  std::vector<Process::ControlInlet*> m_currentControls{};
  std::span<Process::ControlInlet*> m_displayedControls{};

  int m_current_bank_offset{};
  int m_current_channel_offset{};
  struct Controller
  {
    std::shared_ptr<Process::RemoteControlInterface> controller;
    std::vector<ControllerHandle> handles;
    struct ControlMap
    {
      ControllerHandle handle;
      QPointer<Process::ControlInlet> inlet;
    };

    std::vector<ControlMap> maps;
  };

  std::vector<Controller> m_controllers;

  ossia::flat_map<Process::ProcessModel*, std::vector<QMetaObject::Connection>>
      m_currentConnections;
};

}
