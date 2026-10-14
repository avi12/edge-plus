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
#include "control.h"
#include "drag.h"
#include "grip.h"
#include "state.h"

namespace fixedge::hooks {

using namespace fixedge::edge155;

// MultiContentsView::SetWebContentsAtIndex — the feeder's one call. For a grown
// split (index >= the current container count) build the missing container(s)
// first, then run the original. The index<2 guard is lifted at install (see the
// safety patches below). A normal 2-pane split only sees index 0/1, so it is
// unaffected.
using SetWc_t = void*(__fastcall*)(void*, void*, uint32_t);
inline SetWc_t o_setwc = nullptr;
inline void* __fastcall h_setwc(void* mcv, void* web_contents, uint32_t index) {
  if (index >= edge::McvCount(mcv)) {
    Log("[grow] feeder wants index %u; count %llu - building container(s)", index,
        static_cast<unsigned long long>(edge::McvCount(mcv)));
    while (edge::McvCount(mcv) <= index) {
      if (!panes::AddContainer(mcv)) break;
    }
  }
  void* r = o_setwc(mcv, web_contents, index);
  // Every pane in a split is on-screen, but Edge only force-paints the ones it
  // natively tracks; a fed background member stays blank until shown. Show it so
  // panes 2+ (and re-fed members) actually paint. Harmless for the active tab.
  if (web_contents) {
    edge::WebContentsWasShown(web_contents);
    // Edge only watches the first and last member for hover; watch every pane.
    edge::ListenSplitInput(web_contents);
  }
  panes::EnsureControlButtons(mcv, static_cast<int>(index));  // pane 3+ needs a bar
  grip::EnsureGrips(mcv);  // and every pane of a grown split a drag grip
  return r;
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
  const bool bounded =
      edge::Field<uint8_t>(avail, 0x4) == 1 && edge::Field<uint8_t>(avail, 0xc) == 1;
  const int width = edge::Field<int32_t>(avail, 0x0), height = edge::Field<int32_t>(avail, 0x8);
  // Track the content size on every pass (native too) so a resize snaps.
  const bool resized = bounded && anim::NoteHost(mcv, width, height);
  if (edge::McvCount(mcv) <= 2 || !bounded) return o_calc(self, out, avail);  // native 1-2 containers
  // The MCV was grown past two containers. Tile exactly as many as the live split
  // has members (1 once it has been collapsed); surplus containers are hidden.
  const int panes = edge::McvActiveSplitPanes(mcv);
  return geometry::TilePanes(mcv, out, width, height, panes, resized);
}

// MultiContentsView::SetActiveIndex — Edge CHECKs index < 2 (int3 when a pane
// 3+ takes focus) and only clears the primary flag on "the other" of panes 0/1.
// For a grown split, do the same work for N panes: record the index, flag that
// pane primary and every other pane not, then refresh the borders. A stock split
// runs the original.
using SetActive_t = void(__fastcall*)(void*, int);
inline SetActive_t o_set_active = nullptr;
inline void __fastcall h_set_active(void* mcv, int index) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  if (count <= 2) return o_set_active(mcv, index);
  if (index < 0 || index >= count || !edge::ViewGetVisible(edge::McvContainerAt(mcv, index))) {
    Log("[active] ignored SetActiveIndex(%d) of %d", index, count);  // Edge would CHECK-crash
    return;
  }
  edge::Field<int32_t>(mcv, off::MCV_active_index) = index;
  for (int i = 0; i < count; ++i) {
    edge::Field<uint8_t>(edge::ContainerWebView(edge::McvContainerAt(mcv, i)), off::CWV_primary_flag) =
        i == index ? 1 : 0;
  }
  edge::McvUpdateBorders(mcv);
  Log("[active] pane %d of %d", index, count);
}

// views::Button's mouse handlers, shared by every button in Edge. A grip has no
// handler of its own (grip.h): its press starts a pane drag and is claimed, so
// views keeps sending that button the drags and the release with its capture.
// Any other button runs the original untouched.
using ButtonPressed_t = bool(__fastcall*)(void*, const void*);
using ButtonReleased_t = void(__fastcall*)(void*, const void*);
using ButtonCaptureLost_t = void(__fastcall*)(void*);
inline ButtonPressed_t o_pressed = nullptr;
inline ButtonPressed_t o_dragged = nullptr;
inline ButtonReleased_t o_released = nullptr;
inline ButtonCaptureLost_t o_capture_lost = nullptr;

inline bool __fastcall h_pressed(void* button, const void* event) {
  void* mcv = nullptr;
  int pane = -1;
  if (!grip::Locate(button, &mcv, &pane)) {
    return o_pressed(button, event);
  }
  drag::Begin(mcv, pane, edge::EventRootLocation(event));
  return true;
}

inline bool __fastcall h_dragged(void* button, const void* event) {
  if (grip::IsGrip(button)) {
    drag::Move(edge::EventRootLocation(event));
  }
  return o_dragged(button, event);
}

inline void __fastcall h_released(void* button, const void* event) {
  if (grip::IsGrip(button)) {
    drag::Release();
  }
  o_released(button, event);
}

inline void __fastcall h_capture_lost(void* button) {
  if (grip::IsGrip(button)) {
    drag::Cancel();
  }
  o_capture_lost(button);
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
  Install(rva::MultiContentsView_SetActiveIndex, reinterpret_cast<void*>(h_set_active),
          &o_set_active, "SetActiveIndex");
  Install(rva::Button_OnMousePressed, reinterpret_cast<void*>(h_pressed), &o_pressed,
          "Button::OnMousePressed");
  Install(rva::Button_OnMouseDragged, reinterpret_cast<void*>(h_dragged), &o_dragged,
          "Button::OnMouseDragged");
  Install(rva::Button_OnMouseReleased, reinterpret_cast<void*>(h_released), &o_released,
          "Button::OnMouseReleased");
  Install(rva::Button_OnMouseCaptureLost, reinterpret_cast<void*>(h_capture_lost),
          &o_capture_lost, "Button::OnMouseCaptureLost");
  Install(rva::SplitWindowController_DoAction, reinterpret_cast<void*>(collapse::h_do_action),
          &collapse::o_do_action, "DoActionToSplitWindow");
  Install(rva::SplitTabMenuModel_ctor, reinterpret_cast<void*>(menu::h_ctor), &menu::o_ctor,
          "SplitTabMenuModel::ctor");
  Install(rva::SplitTabMenuModel_ExecuteCommand, reinterpret_cast<void*>(menu::h_exec), &menu::o_exec,
          "SplitTabMenuModel::ExecuteCommand");
  Install(rva::WebUISplitTabsControl_HandleContextMenu, reinterpret_cast<void*>(menu::h_handle_ctx),
          &menu::o_handle_ctx, "WebUISplitTabsControl::HandleContextMenu");
  // Safety patches: turn Edge's 2-pane CHECKs into fallbacks. They stay for the
  // life of the process (never restored on unload): they point at nothing of
  // ours, and a split still grown past two when this DLL unloads (hot-reload or
  // toggle-off) would otherwise hit a CHECK as soon as Edge touches pane 3+.
  const uint8_t guard_was = edge::PatchByte(rva::SetWebContentsAtIndex_guard_imm, 0xff);
  Log("[patch] SetWebContentsAtIndex guard %u -> 0xff", guard_was);
  uint8_t was[6] = {};
  edge::PatchNop(rva::SplitA11yLabel_paneidx_jne, 6, was);  // a11y label int3 at index >= 2
  Log("[patch] a11y pane-index check NOPed (was 0x%02x 0x%02x)", was[0], was[1]);
  edge::PatchNop(rva::SetActiveIndex_index_jae, 6, was);  // SetActiveIndex int3 at index >= 2
  Log("[patch] SetActiveIndex index check NOPed (was 0x%02x 0x%02x)", was[0], was[1]);
  control::Install();  // the add-pane control channel (UI-thread trigger)
  return true;
}

inline void RemoveAll() {
  control::Remove();
  MH_DisableHook(MH_ALL_HOOKS);  // the safety patches stay (see InstallAll)
  MH_Uninitialize();
}

}  // namespace fixedge::hooks
