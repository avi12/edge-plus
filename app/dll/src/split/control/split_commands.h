// split_commands.h — the split's commands from the channel (channel.h): add a
// pane beside the active one, give every pane its grip, show a pane's hover
// bar, create or grow a split, and break every grown split up.
#pragma once
#include <windows.h>
#include "chrome/pane/dividers.h"
#include "chrome/pane/grip.h"
#include "core/channel.h"
#include "core/log.h"
#include "edge/api/browser.h"
#include "edge/api/split_controls.h"
#include "edge/api/split_view.h"
#include "edge/api/tab_strip.h"
#include "layout/geometry.h"
#include "layout/pane_tree.h"
#include "layout/state.h"
#include "split/control/browser_windows.h"
#include "split/split_data.h"
#include "split/split_ops.h"

namespace edgeplus::control {

using channel::CodeOf;
using channel::Command;
using channel::OperandOf;

// No grown split may outlive the DLL: stock Edge would mis-render it, so the
// user is left with plain tabs.
inline BOOL CALLBACK BreakUpWindowSplits(HWND hwnd, LPARAM /*unused*/) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  void* mcv = edge::McvForWindow(hwnd);
  void* tsm = mcv ? edge::McvTabStripModel(mcv) : nullptr;
  if (!tsm) {
    return TRUE;
  }
  const int dissolved = split_data::BreakUpGrownSplits(tsm);
  if (dissolved > 0) {
    // Nudge Edge to re-evaluate its split view now the active tab has left the
    // split, so the window starts collapsing before the hooks come off. The
    // grown MCV keeps surplus containers, so the final full-width layout may
    // still settle on the user's next interaction with the window.
    void* active = edge::ActiveTab(tsm);
    edge::MaybeUpdateSplitView(edge::McvBrowserView(mcv),
                               active ? edge::TabContents(active) : nullptr);
    ForgetTree(mcv);
  }
  // Every window, dissolved or not: a split collapsed earlier is in the same
  // state Edge's stock layout cannot take.
  geometry::RestoreStockVisibility(mcv);
  return TRUE;
}

inline void AddPaneByCommand(HWND hwnd, WPARAM wparam) {
  void* mcv = edge::McvForWindow(hwnd);
  Log("[control] msg directional dir=%llu mcv=%p",
      static_cast<unsigned long long>(wparam) - CodeOf(Command::kAddPaneBase),
      mcv);
  if (mcv) {
    split_ops::AddPane(
        mcv, static_cast<Direction>(wparam - CodeOf(Command::kAddPaneBase)));
  }
}

inline void EnsureGripsByCommand(HWND hwnd) {
  void* mcv = edge::McvForWindow(hwnd);
  // A collapsed split keeps its surplus containers; they get no grips.
  const bool isGrownSplit =
      (mcv != nullptr) &&
      edge::McvActiveSplitPanes(mcv) > split_data::kMinSplitMembers;
  if (isGrownSplit) {
    grip::EnsureGrips(mcv);
    dividers::Ensure(mcv);
  }
}

inline void ShowBarByCommand(HWND hwnd, int pane) {
  void* mcv = edge::McvForWindow(hwnd);
  const bool hasPane =
      (mcv != nullptr) && std::cmp_less(pane, edge::McvCount(mcv));
  if (hasPane) {
    void* webContents =
        edge::ContainerWebContents(edge::McvContainerAt(mcv, pane));
    const bool isShown = edge::ShowControlButtonFor(
        edge::BrowserOf(edge::McvBrowserView(mcv)), webContents);
    Log("[control] show bar pane=%d wc=%p -> %d", pane, webContents,
        static_cast<int>(isShown));
  }
}

// A code no command claims: create a split of N panes or of two, or grow one.
inline void CreateOrGrowByCommand(HWND hwnd, WPARAM wparam) {
  void* tsm = edge::TsmForWindow(hwnd);
  Log("[control] message wparam=%llu tsm=%p",
      static_cast<unsigned long long>(wparam), tsm);
  if (!tsm) {
    return;
  }
  const bool isCreatePanes = wparam >= CodeOf(Command::kCreatePanesBase);
  if (isCreatePanes) {
    split_data::CreateSplitN(tsm, OperandOf(wparam, Command::kCreatePanesBase));
    return;
  }
  const bool isCreateSplit = wparam >= CodeOf(Command::kCreateSplit);
  if (isCreateSplit) {
    split_data::CreateSplit(tsm);
    return;
  }
  split_data::GrowSplit(tsm);
}

}  // namespace edgeplus::control
