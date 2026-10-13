// hooks.h — MinHook plumbing and the detours, wiring the modules together.
// One concern: intercept Edge's split methods and delegate to panes/geometry/menu.
// Every detour is gated so a stock (<=2) split runs the original untouched; panes
// grow only on an explicit directional-add command (menu.h), never automatically.
#pragma once
#include <cstdint>
#include "MinHook.h"
#include "edge.h"
#include "panes.h"
#include "geometry.h"
#include "collapse.h"
#include "menu.h"
#include "state.h"

namespace fixedge::hooks {

using namespace fixedge::edge155;

// MultiContentsView::SetWebContentsAtIndex — the feeder's one call. For a grown
// split (index >= the current container count) build the missing container(s)
// first, then run the original. The index<2 guard is lifted at install and
// restored on unload. A normal 2-pane split only sees index 0/1, so it is
// unaffected.
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
// Records the active split's MCV (so a menu command knows what to grow) and tiles
// N panes once the split has grown past two.
using Calc_t = void*(__fastcall*)(void*, void*, void*);
inline Calc_t o_calc = nullptr;
inline void* __fastcall h_calc(void* self, void* out, void* avail) {
  void* mcv = edge::McvFromLayoutBase(self);
  g_active_mcv = mcv;
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
  Install(rva::MultiContentsView_SetWebContentsAtIndex, reinterpret_cast<void*>(h_setwc), &o_setwc,
          "SetWebContentsAtIndex");
  Install(rva::MultiContentsView_CalculateProposedLayout, reinterpret_cast<void*>(h_calc), &o_calc,
          "CalculateProposedLayout");
  Install(rva::SplitWindowController_DoAction, reinterpret_cast<void*>(collapse::h_do_action),
          &collapse::o_do_action, "DoActionToSplitWindow");
  Install(rva::SplitTabMenuModel_ctor, reinterpret_cast<void*>(menu::h_ctor), &menu::o_ctor,
          "SplitTabMenuModel::ctor");
  Install(rva::SplitTabMenuModel_ExecuteCommand, reinterpret_cast<void*>(menu::h_exec), &menu::o_exec,
          "SplitTabMenuModel::ExecuteCommand");
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
