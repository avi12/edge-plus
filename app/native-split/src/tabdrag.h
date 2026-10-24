// tabdrag.h — tab drags and split panes. One rule for every drag of one tab,
// whether it began on the tab list or as a pane's grip drag: over the tab list
// it is Edge's own drag, and crossing a split's tab there picks that split; off
// the list, inside the window, the picked split shows and the docking preview
// shows where the tab would land as a new pane. A drag starts with the split
// that was showing picked: a pane's own, or the one on screen when its tab was
// grabbed. With no split picked the drag is Edge's own. UI thread.
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
// Edge (TakeOverPaneDrag) and docks as any tab's drag does; a cancelled drag
// puts the pane back.
struct PaneOrigin {
  void* tsm = nullptr;
  void* mcv = nullptr;
  void* context = nullptr;  // the tab strip running the drag
  void* draggedContents = nullptr;
  void* splitContents = nullptr;  // a pane that stayed: it shows the split
  Node tree;  // the split as it was, the pane renumbered as its newest
};
inline constinit std::optional<PaneOrigin> g_paneOrigin;

// The split a drag would dock into once off the tab list: the last split tab
// the pointer crossed there, or a pane's own split. It outlives the switches
// between Edge's drag and docking, which reset g_session.
struct PickedSplit {
  void* controller = nullptr;  // the drag it was picked in
  void* splitContents = nullptr;
};
inline constinit PickedSplit g_picked;

// The page of the active tab last laid out, and the one before it. A press on
// a tab activates it before its drag starts, so the split the user was looking
// at when they grabbed the tab is the page before. (Switching tabs does not go
// through MultiContentsView::SetWebContentsAtIndex; the layout always follows.)
struct ShownPages {
  void* current = nullptr;
  void* previous = nullptr;
};
inline constinit ShownPages g_shown;

// Every layout of a window's split view (hooks.h h_calc).
inline void OnLaidOut(void* mcv) {
  void* tsm = edge::McvTabStripModel(mcv);
  void* tab = tsm ? edge::ActiveTab(tsm) : nullptr;
  void* webContents = tab ? edge::TabContents(tab) : nullptr;
  const bool isNewPage = webContents && webContents != g_shown.current;
  if (isNewPage) {
    g_shown = ShownPages{webContents, g_shown.current};
  }
}

// The split shown until the press on the dragged tab, or null. Only compared,
// never dereferenced: the pages may be gone.
inline void* SplitShownBefore(void* controller, void* tsm) {
  const bool isDraggedTabJustShown =
      g_shown.current == edge::DraggedContents(controller, 0);
  void* tab =
      isDraggedTabJustShown ? edge::TabShowing(tsm, g_shown.previous) : nullptr;
  return tab && edge::TabIsSplit(tab) ? g_shown.previous : nullptr;
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
inline int TabUnder(void* context, void* tsm, void* draggedContents,
                    Point screenPoint) {
  const int tabCount = edge::TabCount(tsm);
  for (int i = 0; i < tabCount; i++) {
    void* contents = edge::TabWebContentsAt(tsm, i);
    void* tabView = contents == draggedContents
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

// Called on every move over the tab list, while Edge's drag runs.
inline void PickSplitUnder(void* controller, void* context, void* tsm,
                           Point screenPoint) {
  void* draggedContents = edge::DraggedContents(controller, 0);
  const int index = TabUnder(context, tsm, draggedContents, screenPoint);
  void* tab = index >= 0 ? edge::TabModelAt(tsm, index) : nullptr;
  const bool isSplitTab = tab && edge::TabIsSplit(tab);
  if (!isSplitTab) {
    return;
  }
  void* splitContents = edge::TabWebContentsAt(tsm, index);
  if (g_picked.splitContents != splitContents) {
    Log("[tabdrag] picked the split of tab %d", index);
  }
  g_picked = PickedSplit{controller, splitContents};
}

// The split picked in this drag, or null. A new drag starts with its pane's own
// split, or else the split shown when its tab was grabbed.
inline void* PickedSplitFor(void* controller, void* tsm,
                            const PaneOrigin* pane) {
  if (g_picked.controller != controller) {
    void* splitContents =
        pane ? pane->splitContents : SplitShownBefore(controller, tsm);
    g_picked = PickedSplit{controller, splitContents};
  }
  return g_picked.splitContents;
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
  g_picked = PickedSplit{};
  g_drop.reset();
  g_paneOrigin.reset();
}

// --- a pane's grip drag that reaches the tab list -----------------------------

// Starts Edge's own drag of the tab showing `draggedContents`, as a press on
// that tab would, then hands the strip the mouse capture as the attach of a
// tab dragged in from another window does: from here views sends the strip
// the moves and the release. Returns false if no drag started.
inline bool StartEdgeTabDrag(void* mcv, void* draggedContents,
                             Point rootLocation) {
  void* tsm = edge::McvTabStripModel(mcv);
  void* context = edge::BrowserViewDragContext(edge::McvBrowserView(mcv));
  if (!context) {
    return false;
  }
  // Like a press, which selects the tab: Edge drags the selected tabs.
  edge::ActivateTabAt(tsm, edge::IndexOfWebContents(tsm, draggedContents));
  void* tabView = edge::DragContextTabFor(context, draggedContents);
  if (!tabView) {
    return false;
  }
  // The tab list animates a tab that just left a split's row, and no drag
  // starts while it does. Ending that also puts the tab where it is going.
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
    return false;
  }
  edge::TabDragSetCapture(controller, context);
  return true;
}

// Takes the pane out of its split and drags its tab (StartEdgeTabDrag).
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
  if (!StartEdgeTabDrag(mcv, draggedContents, rootLocation)) {
    Log("[tabdrag] pane %d left its split, but Edge started no drag", pane);
    return false;
  }
  g_paneOrigin = PaneOrigin{
      tsm, mcv, context, draggedContents, splitContents, std::move(tree)};
  Log("[tabdrag] pane %d is a dragged tab now", pane);
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

// Inside the window but off its tab list: where a picked split shows. Out of
// the window Edge tears the tab off as ever.
inline bool IsOverWindowContent(void* context, Point screenPoint) {
  void* mcv = edge::McvForWindow(channel::g_window);
  const bool isInWindow =
      mcv &&
      Contains(edge::ViewBoundsInScreen(edge::ViewRoot(mcv)), screenPoint);
  return isInWindow && !drag::IsOverTabList(context, screenPoint);
}

// TabDragController::ContinueDragging: docking we take the move, since Edge's
// would reorder or tear off the tab; otherwise Edge's own.
using ContinueDragging_t = int(__fastcall*)(void*, const Point*);
inline constinit ContinueDragging_t o_continue_dragging = nullptr;
inline int __fastcall h_continue_dragging(void* controller,
                                          const Point* screenPoint) {
  void* context = edge::DragContext(controller);
  void* tsm = context ? edge::DragContextTabStripModel(context) : nullptr;
  const bool canDock = tsm && CanDock(controller, tsm);
  void* splitContents =
      canDock ? PickedSplitFor(controller, tsm, PaneDraggedBy(context))
              : nullptr;
  const bool isOverTabList =
      canDock && drag::IsOverTabList(context, *screenPoint);
  if (isOverTabList) {
    PickSplitUnder(controller, context, tsm, *screenPoint);
  }
  const bool isDock =
      splitContents && IsOverWindowContent(context, *screenPoint);
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
    Log("[tabdrag] dock mode");
  }
  ShowSplitAt(edge::IndexOfWebContents(tsm, splitContents));
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
  const bool isDropping = isReleased && hasDockTarget;
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
  g_picked = PickedSplit{};
  o_end_drag(controller, reason);
  const bool isDropQueued = isDropping || isPuttingPaneBack;
  if (isDropQueued) {
    channel::Post(channel::CodeOf(channel::Command::kTabDrop));
  }
}

// EventTracker::OnEvent: Alt once switched to docking, and hands used to that
// still hold it; its key events must not end the drag the way every other key
// does.
using TrackerEvent_t = void(__fastcall*)(void*, void*);
inline constinit TrackerEvent_t o_tracker_event = nullptr;
inline void __fastcall h_tracker_event(void* tracker, void* event) {
  const int type = edge::EventType(event);
  const bool isKeyEvent = type == edge155::off::EVENT_type_key_pressed ||
                          type == edge155::off::EVENT_type_key_released;
  const bool isAltKeyEvent = isKeyEvent && IsAltKey(edge::KeyEventCode(event));
  if (isAltKeyEvent) {
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

// --- a script's stand-in for the mouse on a grip or a tab ---------------------
// (channel kGripPressBase, kTabPress, kGripMoveTo, kGripRelease, kGripEscape).
// Points are window DIPs. Each calls what the real event reaches: the grip's
// handlers while the pane's drag runs, then what the strip's OnMouseDragged
// and OnMouseReleased call once the tab list runs the drag.

// The split view of the window whose tab list a script pressed a tab of.
inline constinit void* g_scriptedTabDragMcv = nullptr;

// The split view of the window whose tab list runs a scripted drag, or null.
inline void* ScriptedTabDragMcv() {
  return g_paneOrigin.has_value() ? g_paneOrigin->mcv : g_scriptedTabDragMcv;
}

inline void SimulateGripPress(void* mcv, int pane, Point rootLocation) {
  const bool hasPane = mcv && std::cmp_less(pane, edge::McvCount(mcv));
  if (hasPane) {
    drag::Begin(mcv, pane, rootLocation);
  }
}

// Presses the tab at strip `index` in its middle and starts its drag.
inline void SimulateTabPress(void* mcv, int index) {
  void* tsm = mcv ? edge::McvTabStripModel(mcv) : nullptr;
  void* contents = tsm ? edge::TabWebContentsAt(tsm, index) : nullptr;
  if (!contents) {
    return;
  }
  const bool isStarted = StartEdgeTabDrag(mcv, contents, Point{});
  g_scriptedTabDragMcv = isStarted ? mcv : nullptr;
  Log("[tabdrag] script pressed tab %d: %s", index,
      isStarted ? "dragging" : "no drag");
}

inline void SimulateGripMove(Point rootLocation) {
  if (drag::IsActive()) {
    OnGripDragged(rootLocation);
    return;
  }
  void* mcv = ScriptedTabDragMcv();
  void* context =
      mcv ? edge::BrowserViewDragContext(edge::McvBrowserView(mcv)) : nullptr;
  void* controller = context ? edge::DragContextController(context) : nullptr;
  if (controller) {
    edge::TabDragMoveTo(controller, drag::RootToScreen(mcv, rootLocation));
  }
}

// `reason`: TABDRAG_end_complete (let go) or TABDRAG_end_cancel (Escape).
inline void SimulateGripEnd(int reason) {
  if (drag::IsActive()) {
    if (reason == edge155::off::TABDRAG_end_complete) {
      drag::Release();
    } else {
      drag::Cancel();
    }
    return;
  }
  void* mcv = ScriptedTabDragMcv();
  g_scriptedTabDragMcv = nullptr;
  if (mcv) {
    edge::DragContextEndDrag(
        edge::BrowserViewDragContext(edge::McvBrowserView(mcv)), reason);
  }
}

}  // namespace fixedge::tabdrag
