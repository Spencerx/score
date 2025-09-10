#pragma once
#include <Scenario/Application/Menus/ObjectsActions/EventActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/IntervalActions.hpp>

#include <score/actions/Action.hpp>
#include <score/actions/Menu.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/Selection.hpp>

#include <ossia/detail/json.hpp>
namespace Scenario
{
struct Point;
class ScenarioApplicationPlugin;
class ScenarioDocumentModel;
class ScenarioDocumentPresenter;
class ScenarioPresenter;
class SCORE_PLUGIN_SCENARIO_EXPORT ObjectMenuActions final : public QObject
{
public:
  explicit ObjectMenuActions(ScenarioApplicationPlugin* parent);

  void makeGUIElements(score::GUIElements& ref);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

  CommandDispatcher<> dispatcher() const;

  auto appPlugin() const { return m_parent; }

private:
  void copySelectedElementsToJson(JSONReader& r);
  void removeSelectedElements();
  void cutSelectedElementsToJson(JSONReader& r);

  void pasteElements(QPoint);
  void pasteElements();

  void pasteElements(const rapidjson::Value& obj, const Scenario::Point& origin);
  void pasteElementsAfter(const rapidjson::Value& obj);
  void writeJsonToSelectedElements(const rapidjson::Value& obj);

  bool isFocusingScenario() const noexcept;

  ScenarioDocumentModel* getScenarioDocModel() const;
  ScenarioDocumentPresenter* getScenarioDocPresenter() const;
  ScenarioApplicationPlugin* m_parent{};

  EventActions m_eventActions;
  IntervalActions m_cstrActions;

  QAction* m_removeElements{};
  QAction* m_removeElementsKeepLinked{};
  QAction* m_copyContent{};
  QAction* m_cutContent{};
  QAction* m_pasteElements{};
  QAction* m_pasteElementsAfter{};
  QAction* m_elementsToJson{};
  QAction* m_mergeTimeSyncs{};
  QAction* m_mergeEvents{};
  QAction* m_encapsulate{};
  QAction* m_decapsulate{};
  QAction* m_duplicate{};

  QAction* m_selectAll{};
  QAction* m_deselectAll{};
  QAction* m_selectTop{};
  QAction* m_goToParent{};
};
}
