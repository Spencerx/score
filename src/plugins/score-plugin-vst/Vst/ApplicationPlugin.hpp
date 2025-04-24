#pragma once
#include <Vst/Loader.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QElapsedTimer>
#if QT_CONFIG(process)
#include <QProcess>
#endif
#include <QWebSocketServer>

#include <thread>
#include <verdigris>
namespace vst
{
struct VSTInfo
{
  QString path;
  QString prettyName;
  QString displayName;
  QString author;
  int32_t uniqueID{};
  int32_t controls{};
  bool isSynth{};
  bool isValid{};
};

class Model;
class ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& app);
  void initialize() override;
  ~ApplicationPlugin() override;

  void clearVSTs();
  void rescanVSTs(QStringList);
  void processIncomingMessage(const QString& txt);
  void addInvalidVST(const QString& path);
  void addVST(const QString& path, const QJsonObject& json);

  // Used for idle timers
  void registerRunningVST(vst::Model*);
  void unregisterRunningVST(vst::Model*);

  void scanVSTsEvent();

  void vstChanged() W_SIGNAL(vstChanged)

  std::vector<VSTInfo> vst_infos;
  ossia::hash_map<int32_t, vst::Module*> vst_modules;

  const std::thread::id m_tid{std::this_thread::get_id()};
  auto mainThreadId() const noexcept { return m_tid; }
  std::vector<vst::Model*> m_runningVSTs;

#if QT_CONFIG(process)
  struct ScanningProcess
  {
    QString path;
    std::unique_ptr<QProcess> process;
    bool scanning{};
    QElapsedTimer timer;
  };

  std::vector<ScanningProcess> m_processes;

private:
  QWebSocketServer m_wsServer;
#endif

  void timerEvent(QTimerEvent* event) override;
};

class GUIApplicationPlugin
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  GUIApplicationPlugin(const score::GUIApplicationContext& app);
  void initialize() override;
};
}
