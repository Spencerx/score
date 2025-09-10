#pragma once
#include <Process/Actions/ProcessActions.hpp>
#include <Process/Layer/LayerContextMenu.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/actions/Action.hpp>
#include <score/selection/SelectionStack.hpp>

namespace Scenario
{
class ScenarioInterface;
class ScenarioDocumentModel;
class ProcessModel;
SCORE_PLUGIN_SCENARIO_EXPORT const Scenario::ScenarioInterface*
focusedScenarioInterface(const score::DocumentContext& ctx);
SCORE_PLUGIN_SCENARIO_EXPORT const Scenario::ProcessModel*
focusedScenarioModel(const score::DocumentContext& ctx);

//! Anything in a scenario model
class SCORE_PLUGIN_SCENARIO_EXPORT EnableWhenScenarioModelObject final
    : public score::ActionCondition
{
public:
  EnableWhenScenarioModelObject();

  static score::ActionConditionKey static_key();

private:
  void action(score::ActionManager& mgr, score::MaybeDocument doc) override;
};

//! Only events, nodes, states
class EnableWhenScenarioInterfaceInstantObject final : public score::ActionCondition
{
public:
  EnableWhenScenarioInterfaceInstantObject();

  static score::ActionConditionKey static_key();

private:
  void action(score::ActionManager& mgr, score::MaybeDocument doc) override;
};

//! Anything in a scenario interface
class EnableWhenScenarioInterfaceObject final : public score::ActionCondition
{
public:
  EnableWhenScenarioInterfaceObject();

  static score::ActionConditionKey static_key();

private:
  void action(score::ActionManager& mgr, score::MaybeDocument doc) override;
};
}

/// Conditions relative to Scenario elements
SCORE_DECLARE_DOCUMENT_CONDITION(Scenario::ScenarioDocumentModel)

SCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ProcessModel)
SCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ScenarioInterface)

SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::IntervalModel)
SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::EventModel)
SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::StateModel)
SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::TimeSyncModel)

/// Actions
// View
SCORE_DECLARE_ACTION(SelectAll, "&Select All", Scenario, QKeySequence::SelectAll)
SCORE_DECLARE_ACTION(DeselectAll, "&Deselect All", Scenario, QKeySequence::Deselect)
SCORE_DECLARE_ACTION(
    SelectTop, "Select &Top", Scenario, QKeySequence{QObject::tr("Ctrl+Shift+T")})

// Transport
SCORE_DECLARE_ACTION(Play, "&Play", Scenario, Qt::Key_Space)
SCORE_DECLARE_ACTION(PlayGlobal, "&Play Root", Scenario, QKeySequence("Shift+Space"))
SCORE_DECLARE_ACTION(Stop, "&Stop", Scenario, Qt::Key_Return)
SCORE_DECLARE_ACTION(GoToStart, "&Go to Start", Scenario, Qt::Key_Back)
SCORE_DECLARE_ACTION(GoToEnd, "Go to &End", Scenario, Qt::Key_Forward)
SCORE_DECLARE_ACTION(
    Reinitialize, "&Reinitialize", Scenario, QKeySequence(QObject::tr("Ctrl+Return")))
SCORE_DECLARE_ACTION(Record, "&Record", Scenario, QKeySequence::UnknownKey)

// Edit
SCORE_DECLARE_ACTION(SelectTool, "Tool &Select", Scenario, Qt::Key_S)
SCORE_DECLARE_ACTION_2S(
    CreateTool, "Tool &Create", Scenario, QKeySequence{QObject::tr("A")},
    QKeySequence{QObject::tr("Shift+A")})
SCORE_DECLARE_ACTION(PlayTool, "Tool &Play", Scenario, Qt::Key_P)

// For those two the "shortcuts" are just shift and alt, but this is not supported
// by QKeySequence. See ToolMenuActions.cpp
SCORE_DECLARE_ACTION(LockMode, "&Lock", Scenario, QKeySequence{})
SCORE_DECLARE_ACTION(Scale, "&Scale mode", Scenario, QKeySequence{})

// Object
#if defined(__APPLE__)
SCORE_DECLARE_ACTION(RemoveElements, "&Remove elements", Scenario, Qt::Key_Backspace)
SCORE_DECLARE_ACTION(
    RemoveElementsKeepLinked, "&Remove cable (keep link)", Scenario,
    QKeySequence(QObject::tr("Shift+Backspace")))
#else
SCORE_DECLARE_ACTION(RemoveElements, "&Remove elements", Scenario, Qt::Key_Delete)
SCORE_DECLARE_ACTION(
    RemoveElementsKeepLinked, "&Remove cable (keep link)", Scenario,
    QKeySequence(QObject::tr("Shift+Delete")))
#endif
SCORE_DECLARE_ACTION(CopyContent, "C&opy", Scenario, QKeySequence::Copy)
SCORE_DECLARE_ACTION(CutContent, "C&ut", Scenario, QKeySequence::Cut)
SCORE_DECLARE_ACTION(PasteElements, "&Paste (elements)", Scenario, QKeySequence::Paste)
SCORE_DECLARE_ACTION(
    PasteElementsAfter, "&Paste (after)", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(
    ElementsToJson, "Convert to &JSON", Scenario, QKeySequence::UnknownKey)

// Event
SCORE_DECLARE_ACTION(MergeEvents, "Merge events", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(AddTrigger, "&Enable Trigger", Scenario, Qt::Key_T)
SCORE_DECLARE_ACTION(
    RemoveTrigger, "&Disable Trigger", Scenario, QKeySequence(QObject::tr("Shift+T")))

SCORE_DECLARE_ACTION(AddCondition, "&Add Condition", Scenario, Qt::Key_C)
SCORE_DECLARE_ACTION(
    RemoveCondition, "&Remove Condition", Scenario, QKeySequence(QObject::tr("Shift+C")))

// Interval
SCORE_DECLARE_ACTION(AddProcess, "&Add a process", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(
    MergeTimeSyncs, "&Synchronize", Scenario, QKeySequence(QObject::tr("Shift+M")))
SCORE_DECLARE_ACTION(ShowRacks, "&Show processes", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(HideRacks, "&Hide processes", Scenario, QKeySequence::UnknownKey)

SCORE_DECLARE_ACTION(
    Encapsulate, "&Encapsulate", Scenario, QKeySequence(QObject::tr("Ctrl+Alt+E")))
SCORE_DECLARE_ACTION(
    Decapsulate, "&Decapsulate", Scenario, QKeySequence(QObject::tr("Ctrl+Alt+D")))
SCORE_DECLARE_ACTION(
    FoldIntervals, "Fold intervals", Scenario, QKeySequence(QObject::tr("Ctrl+Alt+F")))
SCORE_DECLARE_ACTION(
    UnfoldIntervals, "Unfold intervals", Scenario,
    QKeySequence(QObject::tr("Ctrl+Alt+U")))
SCORE_DECLARE_ACTION(
    LevelUp, "Go to parent interval", Scenario, QKeySequence(QObject::tr("Ctrl+Alt+Up")))

SCORE_DECLARE_ACTION(
    Duplicate, "&Duplicate", Scenario, QKeySequence(QObject::tr("Alt+D")))

SCORE_DECLARE_ACTION(
    ShowCables, "&Show cables", Dataflow, QKeySequence(QObject::tr("Alt+Shift+G")))

SCORE_DECLARE_ACTION(
    AutoScroll, "&Auto-scroll", Dataflow, QKeySequence(QObject::tr("Alt+Shift+A")))

// Navigation
SCORE_DECLARE_ACTION(MoveUp, "&Move up", Scenario, Qt::UpArrow)
SCORE_DECLARE_ACTION(MoveDown, "&Move down", Scenario, Qt::DownArrow)
SCORE_DECLARE_ACTION(MoveLeft, "&Move left", Scenario, Qt::LeftArrow)
SCORE_DECLARE_ACTION(MoveRight, "&Move right", Scenario, Qt::RightArrow)
SCORE_DECLARE_ACTION(
    GoToParent, "&Go to parent", Scenario, QKeySequence(QObject::tr("Ctrl+Up")))

/// Context menus
SCORE_PROCESS_DECLARE_CONTEXT_MENU(
    SCORE_PLUGIN_SCENARIO_EXPORT, ScenarioObjectContextMenu)
SCORE_PROCESS_DECLARE_CONTEXT_MENU(
    SCORE_PLUGIN_SCENARIO_EXPORT, ScenarioModelContextMenu)
SCORE_PROCESS_DECLARE_CONTEXT_MENU(SCORE_PLUGIN_SCENARIO_EXPORT, IntervalContextMenu)
SCORE_PROCESS_DECLARE_CONTEXT_MENU(SCORE_PLUGIN_SCENARIO_EXPORT, EventContextMenu)
SCORE_PROCESS_DECLARE_CONTEXT_MENU(SCORE_PLUGIN_SCENARIO_EXPORT, StateContextMenu)
