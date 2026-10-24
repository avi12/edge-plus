// tabdrag.h — tab drags and split panes. Alt + drag one tab into a split:
// hovering a split's tab shows that split, and over it the docking preview
// shows where the tab would land as a new pane. A pane dragged by its grip onto
// the tab list goes on as Edge's own drag of its tab, and docks the same way
// back over the split. Otherwise the drag is Edge's own. UI thread.
// docs/re/tab-drag.md.
#pragma once
#include <windows.h>
#include <algorithm>
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
  bool isDocking = false;  // the moves preview a dock, Edge's drag is paused
  void* tsm = nullptr;
  void* draggedContents = nullptr;
  void* splitContents = nullptr;  // the split shown for the dock, if any
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

// A pane whose grip drag reached the tab list: from there its tab is dragged by
// Edge (TakeOverPaneDrag). Off the tab list the drag docks as an Alt drag does;
// a cancelled drag puts the pane back.
struct PaneOrigin {
  void* tsm = nullptr;
  void* mcv = nullptr;
  void* context = nullptr;  // the tab strip running the drag
  void* draggedContents = nullptr;
  void* splitContents = nullptr;  // a pane that stayed: it shows the split
  Node tree;  // the split as it was, the pane renumbered as its newest
};
inline constinit std::optional<PaneOrigin> g_paneOrigin;

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

// The pane whose drag the strip `context` is running, or null.
inline PaneOrigin* PaneDraggedBy(const void* context) {
  const bool isPaneDrag =
      g_paneOrigin.has_value() && context && g_paneOrigin->context == context;
  return isPaneDrag ? &g_paneOrigin.value() : nullptr;
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

inline void ShowSplitAt(int index) {
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

inline void ShowSplitUnder(void* context, Point screenPoint) {
  ShowSplitAt(TabUnder(context, g_session.tsm, screenPoint));
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
inline void LeaveDockMode() {
  overlay::Hide();
  const int draggedIndex =
      edge::IndexOfWebContents(g_session.tsm, g_session.draggedContents);
  const bool hasShownSplit = g_session.splitContents && draggedIndex >= 0;
  if (hasShownSplit) {
    edge::ActivateTabAt(g_session.tsm, draggedIndex);
  }
  g_session = Session{};
  Log("[tabdrag] Edge's drag again");
}

inline void Cancel() {
  overlay::Hide();
  g_session = Session{};
  g_lastMove = LastMove{};
  g_drop.reset();
  g_paneOrigin.reset();
}

// --- a pane's grip drag that reaches the tab list -----------------------------

// Takes the pane out of its split and starts Edge's own drag of its tab, as a
// press on that tab would, then hands the strip the mouse capture as the
// attach of a tab dragged in from another window does: from here views sends
// the strip the moves and the release. Returns false if no drag started.
inline bool TakeOverPaneDrag(void* mcv, int pane, Point rootLocation) {
  void* tsm = edge::McvTabStripModel(mcv);
  void* context = edge::BrowserViewDragContext(edge::McvBrowserView(mcv));
  const int panes = edge::McvActiveSplitPanes(mcv);
  const bool hasStrip = tsm && context && pane < panes;
  if (!hasStrip) {
    return false;
  }
  Node tree = geometry::LiveTree(mcv, panes);
  RenumberAsNewest(tree, pane, panes - 1);
  void* draggedContents =
      edge::ContainerWebContents(edge::McvContainerAt(mcv, pane));
  void* splitContents =
      edge::ContainerWebContents(edge::McvContainerAt(mcv, pane == 0 ? 1 : 0));
  if (!split_ops::DetachPane(mcv, pane)) {
    return false;
  }
  // Like a press, which selects the tab: Edge drags the selected tabs.
  edge::ActivateTabAt(tsm, edge::IndexOfWebContents(tsm, draggedContents));
  void* tabView = edge::DragContextTabFor(context, draggedContents);
  if (!tabView) {
    Log("[tabdrag] pane %d left its split, but its tab has no view", pane);
    return false;
  }
  // The tab list animates the tab out of the split's row, and no drag starts
  // while it does. Ending that also puts the tab where it is going.
  edge::TabStripStopAnimating(
      edge::BrowserViewTabStrip(edge::McvBrowserView(mcv)));
  Point grabPoint = rootLocation;
  edge::ConvertPoint(edge::ViewRoot(tabView), tabView, &grabPoint);
  const Rect tabBounds = edge::ViewBounds(tabView);
  grabPoint.x = std::clamp(grabPoint.x, 0, std::max(tabBounds.width - 1, 0));
  grabPoint.y = tabBounds.height / 2;
  edge::StartTabDrag(tabView, grabPoint);
  void* controller = edge::DragContextController(context);
  if (!controller) {
    Log("[tabdrag] pane %d left its split, but Edge started no drag", pane);
    return false;
  }
  edge::TabDragSetCapture(controller, context);
  g_paneOrigin = PaneOrigin{
      tsm, mcv, context, draggedContents, splitContents, std::move(tree)};
  const Rect listBounds = edge::ViewBoundsInScreen(context);
  Log("[tabdrag] pane %d is a dragged tab now (grab %d,%d; list %d,%d %dx%d)",
      pane, grabPoint.x, grabPoint.y, listBounds.x, listBounds.y,
      listBounds.width, listBounds.height);
  return true;
}

// A grip's every drag move (hooks.h): the pane's own drag until it reaches the
// tab list, which then takes it over.
inline void OnGripDragged(Point rootLocation) {
  if (!drag::IsReadyForTabList(rootLocation)) {
    drag::Move(rootLocation);
    return;
  }
  const drag::HandedOff handedOff = drag::HandOff();
  TakeOverPaneDrag(handedOff.mcv, handedOff.pane, rootLocation);
}

// --- the detours -------------------------------------------------------------

// TabDragController::ContinueDragging: docking (Alt held, or a pane's drag off
// the tab list) we take the move, since Edge's would reorder or tear off the
// tab; otherwise Edge's own.
using ContinueDragging_t = int(__fastcall*)(void*, const Point*);
inline constinit ContinueDragging_t o_continue_dragging = nullptr;
inline int __fastcall h_continue_dragging(void* controller,
                                          const Point* screenPoint) {
  g_lastMove = LastMove{controller, *screenPoint};
  void* context = edge::DragContext(controller);
  void* tsm = context ? edge::DragContextTabStripModel(context) : nullptr;
  const PaneOrigin* pane = PaneDraggedBy(context);
  const bool isPaneOffTabList =
      pane && !drag::IsOverTabList(context, *screenPoint);
  const bool isDock =
      tsm && (IsAltHeld() || isPaneOffTabList) && CanDock(controller, tsm);
  if (!isDock) {
    if (g_session.isDocking) {
      LeaveDockMode();
    }
    return o_continue_dragging(controller, screenPoint);
  }
  if (!g_session.isDocking) {
    g_session = Session{};
    g_session.isDocking = true;
    g_session.tsm = tsm;
    g_session.draggedContents = edge::DraggedContents(controller, 0);
    Log("[tabdrag] dock mode (%s)", isPaneOffTabList ? "a pane" : "Alt");
  }
  if (isPaneOffTabList) {
    ShowSplitAt(edge::IndexOfWebContents(tsm, pane->splitContents));
  } else {
    ShowSplitUnder(context, *screenPoint);
  }
  PreviewDock(*screenPoint);
  return 0;
}

// TabDragController::EndDrag: the drop is queued, not applied, since changing
// the strip mid-drag ends the drag.
using EndDrag_t = void(__fastcall*)(void*, int);
inline constinit EndDrag_t o_end_drag = nullptr;
inline void __fastcall h_end_drag(void* controller, int reason) {
  PaneOrigin* pane = PaneDraggedBy(edge::DragContext(controller));
  const bool isPaneDrag = pane != nullptr;
  const bool isReleased = reason == edge155::off::TABDRAG_end_complete;
  const bool hasDockTarget = g_session.isDocking && g_session.splitContents &&
                             g_session.plan.zone != dock::Zone::kNone;
  const bool isDropping =
      isReleased && hasDockTarget && (IsAltHeld() || isPaneDrag);
  // As before the tab list took the drag over: let go anywhere but on a dock
  // target and the pane stays a tab of its own (where Edge has its tab); Esc,
  // or a lost capture, puts it back.
  const bool isPuttingPaneBack = isPaneDrag && !isReleased;
  if (isDropping) {
    g_drop = Drop{g_session.tsm, g_session.mcv, g_session.draggedContents,
                  g_session.splitContents, std::move(g_session.plan.result)};
  } else if (isPuttingPaneBack) {
    g_drop = Drop{pane->tsm, pane->mcv, pane->draggedContents,
                  pane->splitContents, std::move(pane->tree)};
  }
  if (isPaneDrag) {
    const char* outcome = "a tab of its own";
    if (isDropping) {
      outcome = "docked";
    } else if (isPuttingPaneBack) {
      outcome = "the pane goes back";
    }
    Log("[tabdrag] the pane's tab drag ended (reason %d): %s", reason, outcome);
    g_paneOrigin.reset();
  }
  overlay::Hide();
  g_session = Session{};
  g_lastMove = LastMove{};
  o_end_drag(controller, reason);
  const bool isDropQueued = isDropping || isPuttingPaneBack;
  if (isDropQueued) {
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
