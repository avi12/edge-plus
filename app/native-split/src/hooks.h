// hooks.h — MinHook plumbing and the three detours, wiring the modules together.
// One concern: intercept Edge's split methods and delegate to panes/geometry.
// Every detour is gated so a stock (<=2) split runs the original untouched.
#pragma once
#include <cstdint>
#include "MinHook.h"
#include "edge.h"
#include "panes.h"
#include "tabs.h"
#include "geometry.h"
#include "collapse.h"

namespace fixedge::hooks {

using namespace fixedge::edge155;  // rva::

// Target pane count for a shown split. <=2 means "leave Edge stock".
inline int g_target_panes = 1;

// BrowserView::ShowSplitView — the one pane feeder. Grow after it feeds the real
// tabs, then invalidate so the layout detour tiles the grown set.
using ShowSplit_t = void*(__fastcall*)(void*);
inline ShowSplit_t o_show = nullptr;
inline void* __fastcall h_show(void* browser_view) {
  void* result = o_show(browser_view);
  if (g_target_panes <= 2) return result;
  void* mcv = edge::BrowserViewMcv(browser_view);
  if (!mcv) return result;
  const size_t before = edge::McvCount(mcv);
  if (before < 1 || before >= static_cast<size_t>(g_target_panes)) return result;
  const size_t after = panes::GrowToPanes(mcv, g_target_panes);
  Log("[grow] ShowSplitView mcv=%p %llu -> %llu", mcv,
      static_cast<unsigned long long>(before), static_cast<unsigned long long>(after));

  // Give each grown pane real content from a background tab (from the strip's
  // tail, so we never steal the active or split-partner tabs). Panes with no
  // spare tab stay blank - no crash. The product's directional-add will instead
  // hand each new pane a freshly opened tab.
  void* tsm = edge::TabStripModelOf(edge::BrowserOf(browser_view));
  const int tab_count = tsm ? edge::TabCount(tsm) : 0;
  for (size_t i = before; i < after; ++i) {
    const int tab_index = tab_count - 1 - static_cast<int>(i - before);
    if (tab_index < static_cast<int>(before)) break;  // ran out of background tabs
    if (tabs::FillPane(browser_view, edge::McvContainerAt(mcv, static_cast<int>(i)), tab_index)) {
      Log("[feed] pane %llu <- tab %d", static_cast<unsigned long long>(i), tab_index);
    }
  }
  edge::InvalidateLayout(mcv);
  return result;
}

// MultiContentsView::SetWebContentsAtIndex — the feeder's one call. For a grown
// split (index >= the current container count) build the missing container(s)
// first, then let the original run. The original's own `index < 2` guard is
// lifted at install (SetWebContentsAtIndex_guard_imm 2 -> 0xff) and restored on
// unload, so feeding pane 2+ no longer traps. A normal 2-pane split only ever
// sees index 0/1, so nothing changes for it.
using SetWc_t = void*(__fastcall*)(void*, void*, uint32_t);
inline SetWc_t o_setwc = nullptr;
inline uint8_t g_guard_saved = 0;
inline void* __fastcall h_setwc(void* mcv, void* web_contents, uint32_t index) {
  if (index >= edge::McvCount(mcv)) {
    Log("[grow] feeder wants index %u; count %llu - building container(s)", index,
        static_cast<unsigned long long>(edge::McvCount(mcv)));
    while (edge::McvCount(mcv) <= index) {
      if (!panes::AddContainer(mcv)) break;
    }
  }
  return o_setwc(mcv, web_contents, index);
}

// LayoutManagerBase::CalculateProposedLayout — `self` is the subobject at
// MCV+0x340; `avail` is a const SizeBounds& {width@0, has@4, height@8, has@0xc}.
using Calc_t = void*(__fastcall*)(void*, void*, void*);
inline Calc_t o_calc = nullptr;
inline void* __fastcall h_calc(void* self, void* out, void* avail) {
  void* mcv = edge::McvFromLayoutBase(self);
  if (edge::McvCount(mcv) <= 2) return o_calc(self, out, avail);
  const bool bounded =
      edge::Field<uint8_t>(avail, 0x4) == 1 && edge::Field<uint8_t>(avail, 0xc) == 1;
  if (!bounded) return o_calc(self, out, avail);
  return geometry::TilePanes(mcv, out, edge::Field<int32_t>(avail, 0x0),
                             edge::Field<int32_t>(avail, 0x8));
}

template <class T>
inline bool Install(uintptr_t rva, void* detour, T** original, const char* name) {
  if (MH_CreateHook(edge::At(rva), detour, reinterpret_cast<void**>(original)) != MH_OK ||
      MH_EnableHook(edge::At(rva)) != MH_OK) {
    Log("[err] hook %s failed", name);
    return false;
  }
  Log("[ok] hooked %s", name);
  return true;
}

inline bool InstallAll() {
  if (MH_Initialize() != MH_OK) {
    Log("[err] MH_Initialize failed");
    return false;
  }
  Install(rva::BrowserView_ShowSplitView, reinterpret_cast<void*>(h_show), &o_show, "ShowSplitView");
  Install(rva::MultiContentsView_SetWebContentsAtIndex, reinterpret_cast<void*>(h_setwc), &o_setwc,
          "SetWebContentsAtIndex");
  Install(rva::MultiContentsView_CalculateProposedLayout, reinterpret_cast<void*>(h_calc), &o_calc,
          "CalculateProposedLayout");
  Install(rva::SplitWindowController_DoAction, reinterpret_cast<void*>(collapse::h_do_action),
          &collapse::o_do_action, "DoActionToSplitWindow");
  // Lift the index<2 guard so the feeder can fill pane 2+ (restored on unload).
  g_guard_saved = edge::PatchByte(rva::SetWebContentsAtIndex_guard_imm, 0xff);
  Log("[patch] SetWebContentsAtIndex guard %u -> 0xff", g_guard_saved);
  return true;
}

inline void RemoveAll() {
  if (g_guard_saved) edge::PatchByte(rva::SetWebContentsAtIndex_guard_imm, g_guard_saved);
  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();
}

}  // namespace fixedge::hooks
