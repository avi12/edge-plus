// hooks.h — MinHook plumbing and the detours, wiring the modules together.
// One concern: intercept Edge's split methods and delegate to panes/geometry/menu.
// Every detour is gated so a stock (<=2) split runs the original untouched; panes
// grow only on an explicit directional-add command (menu.h, control.h), never
// automatically.
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

// Each 2-pane CHECK that InstallAll NOPs is a `jcc rel32` (0F 8x + a 4-byte
// offset; see offsets.h).
inline constexpr size_t kCheckJumpLength = 6;
// SetWebContentsAtIndex's `cmp index, 2` becomes `cmp index, 0xff`.
inline constexpr uint8_t kLiftedIndexCap = 0xff;

// MultiContentsView::SetWebContentsAtIndex — the feeder's one call. For a grown
// split (index >= the current container count) build the missing container(s)
// first, then run the original. The index<2 guard is lifted at install (see the
// safety patches below). A normal 2-pane split only sees index 0/1, so it is
// unaffected.
using SetWc_t = void*(__fastcall*)(void*, void*, uint32_t);
inline SetWc_t o_setwc = nullptr;
inline void* __fastcall h_setwc(void* mcv, void* webContents, uint32_t index) {
  const bool shouldBuildContainers = index >= edge::McvCount(mcv);
  if (shouldBuildContainers) {
    Log("[grow] feeder wants index %u; count %llu - building container(s)",
        index, static_cast<unsigned long long>(edge::McvCount(mcv)));
    while (edge::McvCount(mcv) <= index) {
      const bool isAdded = panes::AddContainer(mcv);
      if (!isAdded) {
        break;
      }
    }
  }
  void* result = o_setwc(mcv, webContents, index);
  // Every pane in a split is on-screen, but Edge only force-paints the ones it
  // natively tracks; a fed background member stays blank until shown. Show it so
  // panes 2+ (and re-fed members) actually paint. Harmless for the active tab.
  if (webContents) {
    edge::WebContentsWasShown(webContents);
    // Edge only watches the first and last member for hover; watch every pane.
    edge::ListenSplitInput(webContents);
  }
  // Pane 3+ needs a hover bar, and every pane of a grown split a drag grip.
  panes::EnsureControlButtons(mcv, static_cast<int>(index));
  grip::EnsureGrips(mcv);
  return result;
}

// LayoutManagerBase::CalculateProposedLayout — `self` is the subobject at
// MCV+0x340; `sizeBounds` is a const SizeBounds& {width@0, has@4, height@8,
// has@0xc}. Records the active split's MCV (so a menu command knows what to
// grow) and tiles N panes once the split has grown past two.
using Calc_t = void*(__fastcall*)(void*, void*, void*);
inline Calc_t o_calc = nullptr;
inline void* __fastcall h_calc(void* self, void* out, void* sizeBounds) {
  void* mcv = edge::McvFromLayoutBase(self);
  g_activeMcv = mcv;
  const bool isBounded = edge::Field<uint8_t>(sizeBounds, 0x4) == 1 &&
                         edge::Field<uint8_t>(sizeBounds, 0xc) == 1;
  const int width = edge::Field<int32_t>(sizeBounds, 0x0);
  const int height = edge::Field<int32_t>(sizeBounds, 0x8);
  // Track the content size on every pass (native too) so a resize snaps.
  const bool isResized = isBounded && anim::NoteHost(mcv, width, height);
  const bool isNativeLayout = edge::McvCount(mcv) <= 2 || !isBounded;
  if (isNativeLayout) {
    return o_calc(self, out, sizeBounds);
  }
  // The MCV was grown past two containers. Tile exactly as many as the live split
  // has members (1 once it has been collapsed); surplus containers are hidden.
  const int paneCount = edge::McvActiveSplitPanes(mcv);
  return geometry::TilePanes(mcv, out, width, height, paneCount, isResized);
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
  const bool isStockSplit = count <= 2;
  if (isStockSplit) {
    return o_set_active(mcv, index);
  }
  const bool isVisiblePane =
      index >= 0 && index < count &&
      edge::ViewGetVisible(edge::McvContainerAt(mcv, index));
  if (!isVisiblePane) {
    // Edge would CHECK-crash on it.
    Log("[active] ignored SetActiveIndex(%d) of %d", index, count);
    return;
  }
  edge::Field<int32_t>(mcv, off::MCV_active_index) = index;
  for (int i = 0; i < count; i++) {
    edge::Field<uint8_t>(edge::ContainerWebView(edge::McvContainerAt(mcv, i)),
                         off::CWV_primary_flag) = i == index ? 1 : 0;
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
  const bool isGrip = grip::Locate(button, &mcv, &pane);
  if (!isGrip) {
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

// ControlButtons::HandleButtonClick(bar, button): Edge's X on pane 2+ closes the
// split's last tab and dissolves the split. In a split of three or more the X
// closes exactly its own pane instead (split_ops::ClosePane). Every other click,
// and a two-pane split, runs the original.
using ButtonClick_t = void(__fastcall*)(void*, void*);
inline ButtonClick_t o_button_click = nullptr;
inline void __fastcall h_button_click(void* bar, void* button) {
  const bool isCloseButton = button == edge::ControlButtonsCloseButton(bar);
  void* mcv = nullptr;
  int pane = -1;
  const bool isClosingOwnPane = isCloseButton &&
                                grip::LocateBar(bar, &mcv, &pane) &&
                                split_ops::ClosePane(mcv, pane);
  if (isClosingOwnPane) {
    return;
  }
  o_button_click(bar, button);
}

template <class T>
inline bool Install(uintptr_t rva, void* detour, T** original,
                    const char* name) {
  const bool isHooked =
      MH_CreateHook(edge::At(rva), detour,
                    reinterpret_cast<void**>(original)) == MH_OK &&
      MH_EnableHook(edge::At(rva)) == MH_OK;
  if (!isHooked) {
    Log("[err] hook %s failed", name);
    return false;
  }
  Log("[ok] hooked %s", name);
  return true;
}

inline bool InstallAll() {
  const bool isInitialized = MH_Initialize() == MH_OK;
  if (!isInitialized) {
    Log("[err] MH_Initialize failed");
    return false;
  }
  Install(rva::MultiContentsView_SetWebContentsAtIndex,
          reinterpret_cast<void*>(h_setwc), &o_setwc, "SetWebContentsAtIndex");
  Install(rva::MultiContentsView_CalculateProposedLayout,
          reinterpret_cast<void*>(h_calc), &o_calc, "CalculateProposedLayout");
  Install(rva::MultiContentsView_SetActiveIndex,
          reinterpret_cast<void*>(h_set_active), &o_set_active,
          "SetActiveIndex");
  Install(rva::ControlButtons_HandleButtonClick,
          reinterpret_cast<void*>(h_button_click), &o_button_click,
          "ControlButtons::HandleButtonClick");
  Install(rva::Button_OnMousePressed, reinterpret_cast<void*>(h_pressed),
          &o_pressed, "Button::OnMousePressed");
  Install(rva::Button_OnMouseDragged, reinterpret_cast<void*>(h_dragged),
          &o_dragged, "Button::OnMouseDragged");
  Install(rva::Button_OnMouseReleased, reinterpret_cast<void*>(h_released),
          &o_released, "Button::OnMouseReleased");
  Install(rva::Button_OnMouseCaptureLost,
          reinterpret_cast<void*>(h_capture_lost), &o_capture_lost,
          "Button::OnMouseCaptureLost");
  Install(rva::SplitWindowController_DoAction,
          reinterpret_cast<void*>(collapse::h_do_action),
          &collapse::o_do_action, "DoActionToSplitWindow");
  Install(rva::SplitTabMenuModel_ctor, reinterpret_cast<void*>(menu::h_ctor),
          &menu::o_ctor, "SplitTabMenuModel::ctor");
  Install(rva::SplitTabMenuModel_ExecuteCommand,
          reinterpret_cast<void*>(menu::h_exec), &menu::o_exec,
          "SplitTabMenuModel::ExecuteCommand");
  Install(rva::WebUISplitTabsControl_HandleContextMenu,
          reinterpret_cast<void*>(menu::h_handle_ctx), &menu::o_handle_ctx,
          "WebUISplitTabsControl::HandleContextMenu");
  // Safety patches: turn Edge's 2-pane CHECKs into fallbacks. They stay for the
  // life of the process (never restored on unload): they point at nothing of
  // ours, and a split still grown past two when this DLL unloads (hot-reload or
  // toggle-off) would otherwise hit a CHECK as soon as Edge touches pane 3+.
  const uint8_t previousGuard =
      edge::PatchByte(rva::SetWebContentsAtIndex_guard_imm, kLiftedIndexCap);
  Log("[patch] SetWebContentsAtIndex guard %u -> 0xff", previousGuard);
  uint8_t previousBytes[kCheckJumpLength] = {};
  // The a11y label's int3 at index >= 2.
  edge::PatchNop(rva::SplitA11yLabel_paneidx_jne, kCheckJumpLength,
                 previousBytes);
  Log("[patch] a11y pane-index check NOPed (was 0x%02x 0x%02x)",
      previousBytes[0], previousBytes[1]);
  // SetActiveIndex's int3 at index >= 2.
  edge::PatchNop(rva::SetActiveIndex_index_jae, kCheckJumpLength,
                 previousBytes);
  Log("[patch] SetActiveIndex index check NOPed (was 0x%02x 0x%02x)",
      previousBytes[0], previousBytes[1]);
  control::Install();  // the add-pane control channel (UI-thread trigger)
  return true;
}

inline void RemoveAll() {
  control::Remove();
  MH_DisableHook(MH_ALL_HOOKS);  // the safety patches stay (see InstallAll)
  MH_Uninitialize();
}

}  // namespace fixedge::hooks
