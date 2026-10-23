// tabdrag.h — Alt + drag one tab into a split: hovering a split's tab shows that
// split, and over it the docking preview shows where the tab would land as a
// new pane. Without Alt the drag is Edge's own. UI thread. docs/re/tab-drag.md.
#pragma once
#include <windows.h>
#include <optional>
#include <utility>
#include "channel.h"
#include "dock.h"
#include "drag.h"
#include "edge.h"
#include "geometry.h"
#include "overlay.h"
#include "split_ops.h"
#include "state.h"

namespace fixedge::tabdrag {

struct Session {
  bool isAltMode = false;
  void* tsm = nullptr;
  void* draggedContents = nullptr;
  void* splitContents = nullptr;  // the split tab hovered into view, if any
  void* mcv = nullptr;            // the split view the plan is for
  dock::Plan plan;                // what releasing now would do
};
inline constinit Session g_session;

// Applied once the drag has finished ending (channel kTabDrop).
struct Drop {
  void* tsm = nullptr;
  void* mcv = nullptr;
  void* draggedContents = nullptr;
  void* splitContents = nullptr;
  Node tree;  // the split's layout with the dragged tab as its newest pane
};
inline constinit std::optional<Drop> g_drop;

// A drag only advances on pointer moves, so Alt pressed or released while the
// pointer rests would change nothing until it moved; the key event replays this.
struct LastMove {
  void* controller = nullptr;
  Point screenPoint{};
};
inline constinit LastMove g_lastMove;

inline bool IsAltHeld() {
  return GetKeyState(VK_MENU) < 0;
}

inline bool IsAltKey(int keyCode) {
  return keyCode == VK_MENU || keyCode == VK_LMENU || keyCode == VK_RMENU;
}

inline bool CanDock(void* controller, void* tsm) {
  const bool isSingleTabDrag = edge::IsDraggingTabs(controller) &&
                               edge::DraggedTabCount(controller) == 1;
  if (!isSingleTabDrag) {
    return false;
  }
  void* contents = edge::DraggedContents(controller, 0);
  void* tab = edge::TabShowing(tsm, contents);
  return tab && !edge::TabIsSplit(tab);
}

// Skips the dragged tab itself; -1 if none.
inline int TabUnder(void* context, void* tsm, Point screenPoint) {
  const int tabCount = edge::TabCount(tsm);
  for (int i = 0; i < tabCount; i++) {
    void* contents = edge::TabWebContentsAt(tsm, i);
    void* tabView = contents == g_session.draggedContents
                        ? nullptr
                        : edge::DragContextTabFor(context, contents);
    const bool isUnderPointer =
        tabView && Contains(edge::ViewBoundsInScreen(tabView), screenPoint);
    if (isUnderPointer) {
      return i;
    }
  }
  return -1;
}

inline void ShowSplitUnder(void* context, Point screenPoint) {
  const int index = TabUnder(context, g_session.tsm, screenPoint);
  void* tab = edge::TabModelAt(g_session.tsm, index);
  const bool isHiddenSplitTab =
      tab && edge::TabIsSplit(tab) && tab != edge::ActiveTab(g_session.tsm);
  if (!isHiddenSplitTab) {
    return;
  }
  edge::ActivateTabAt(g_session.tsm, index);
  g_session.splitContents = edge::TabWebContentsAt(g_session.tsm, index);
  Log("[tabdrag] showing the split of tab %d", index);
}

inline void PreviewDock(Point screenPoint) {
  void* mcv = edge::McvForWindow(channel::g_window);
  const int panes = mcv ? edge::McvActiveSplitPanes(mcv) : 0;
  const bool isOwnSplitShown =
      panes >= split_data::kMinSplitMembers &&
      edge::TsmForWindow(channel::g_window) == g_session.tsm;
  if (!isOwnSplitShown) {
    g_session.plan = {};
    overlay::Hide();
    return;
  }
  Point point = screenPoint;
  edge::ConvertPointFromScreen(mcv, &point);
  const Node tree = geometry::LiveTree(mcv, panes);
  // The tab becomes the split's newest pane: leaf `panes` (GrowSplit).
  g_session.plan = dock::ResolveIncoming(tree, geometry::SplitArea(mcv), point,
                                         panes, g_session.plan);
  g_session.mcv = mcv;
  const bool hasDropTarget = g_session.plan.zone != dock::Zone::kNone;
  if (hasDropTarget) {
    overlay::Show(channel::g_window,
                  drag::SplitToScreen(mcv, g_session.plan.preview),
                  drag::StyleOf(g_session.plan.zone));
  } else {
    overlay::Hide();
  }
}

// Edge's drag expects the dragged tab active again.
inline void LeaveAltMode() {
  overlay::Hide();
  const int draggedIndex =
      edge::IndexOfWebContents(g_session.tsm, g_session.draggedContents);
  const bool hasShownSplit = g_session.splitContents && draggedIndex >= 0;
  if (hasShownSplit) {
    edge::ActivateTabAt(g_session.tsm, draggedIndex);
  }
  g_session = Session{};
  Log("[tabdrag] Alt released: Edge's drag again");
}

inline void Cancel() {
  overlay::Hide();
  g_session = Session{};
  g_lastMove = LastMove{};
  g_drop.reset();
}

// --- the detours -------------------------------------------------------------

// TabDragController::ContinueDragging: in Alt mode we take the move (Edge's
// would reorder or tear off the tab), else Edge's own.
using ContinueDragging_t = int(__fastcall*)(void*, const Point*);
inline constinit ContinueDragging_t o_continue_dragging = nullptr;
inline int __fastcall h_continue_dragging(void* controller,
                                          const Point* screenPoint) {
  g_lastMove = LastMove{controller, *screenPoint};
  void* context = edge::DragContext(controller);
  void* tsm = context ? edge::DragContextTabStripModel(context) : nullptr;
  const bool isAltDock = tsm && IsAltHeld() && CanDock(controller, tsm);
  if (!isAltDock) {
    if (g_session.isAltMode) {
      LeaveAltMode();
    }
    return o_continue_dragging(controller, screenPoint);
  }
  if (!g_session.isAltMode) {
    g_session = Session{};
    g_session.isAltMode = true;
    g_session.tsm = tsm;
    g_session.draggedContents = edge::DraggedContents(controller, 0);
    Log("[tabdrag] Alt held: dock mode");
  }
  ShowSplitUnder(context, *screenPoint);
  PreviewDock(*screenPoint);
  return 0;
}

// TabDragController::EndDrag: the drop is queued, not applied, since changing
// the strip mid-drag ends the drag.
using EndDrag_t = void(__fastcall*)(void*, int);
inline constinit EndDrag_t o_end_drag = nullptr;
inline void __fastcall h_end_drag(void* controller, int reason) {
  const bool isDropping = g_session.isAltMode &&
                          reason == edge155::off::TABDRAG_end_complete &&
                          IsAltHeld() && g_session.splitContents &&
                          g_session.plan.zone != dock::Zone::kNone;
  if (isDropping) {
    g_drop = Drop{g_session.tsm, g_session.mcv, g_session.draggedContents,
                  g_session.splitContents, std::move(g_session.plan.result)};
  }
  overlay::Hide();
  g_session = Session{};
  g_lastMove = LastMove{};
  o_end_drag(controller, reason);
  if (isDropping) {
    channel::Post(channel::CodeOf(channel::Command::kTabDrop));
  }
}

// EventTracker::OnEvent: Alt's key events switch modes; they must not end the
// drag the way every other key does.
using TrackerEvent_t = void(__fastcall*)(void*, void*);
inline constinit TrackerEvent_t o_tracker_event = nullptr;
inline void __fastcall h_tracker_event(void* tracker, void* event) {
  const int type = edge::EventType(event);
  const bool isKeyEvent = type == edge155::off::EVENT_type_key_pressed ||
                          type == edge155::off::EVENT_type_key_released;
  const bool isAltKeyEvent = isKeyEvent && IsAltKey(edge::KeyEventCode(event));
  if (isAltKeyEvent) {
    const bool isDragRunning = g_lastMove.controller != nullptr;
    if (isDragRunning) {
      const Point restingPoint = g_lastMove.screenPoint;
      h_continue_dragging(g_lastMove.controller, &restingPoint);
    }
    return;
  }
  o_tracker_event(tracker, event);
}

// --- the drop (channel kTabDrop, after EndDrag returned) -------------------

inline void ApplyDrop() {
  if (!g_drop) {
    return;
  }
  Drop drop = std::move(*g_drop);
  g_drop.reset();
  const int splitIndex = edge::IndexOfWebContents(drop.tsm, drop.splitContents);
  void* draggedTab = edge::TabShowing(drop.tsm, drop.draggedContents);
  const bool hasBothTabs = splitIndex >= 0 && draggedTab;
  if (!hasBothTabs) {
    Log("[tabdrag] drop: a tab went away");
    return;
  }
  edge::ActivateTabAt(drop.tsm, splitIndex);
  anim::g_isNextEntryInPlace = true;
  const bool hasGrown = split_ops::GrowWithTree(
      drop.mcv, drop.tsm, std::move(drop.tree), draggedTab);
  Log("[tabdrag] drop: %s", hasGrown ? "the tab is a new pane" : "failed");
}

}  // namespace fixedge::tabdrag
