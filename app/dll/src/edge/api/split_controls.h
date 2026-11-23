// split_controls.h — a pane's hover controls (ControlButtons), and the
// SplitWindowController that shows them and serves a split's starting pages.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/api/views.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline void* ContainerControlButtons(void* container) {
  return Field<void*>(container, edge155::off::ECCV_control_buttons);
}

// EdgeContentsContainerView::SetControlButtons(unique_ptr<ControlButtons>*), a
// virtual on its control-buttons interface subobject. The callee takes the bar
// out of `*ownedBar` and nulls it.
inline void ContainerSetControlButtons(void* container, void** ownedBar) {
  void* buttonsInterface =
      reinterpret_cast<uint8_t*>(container) + edge155::off::ECCV_buttons_iface;
  using Fn = void(__fastcall*)(void*, void**);
  reinterpret_cast<Fn>(VirtualFunction(
      buttonsInterface, edge155::off::ECCV_vtbl_SetControlButtons))(
      buttonsInterface, ownedBar);
}

// Build a ControlButtons bar and hand it to `container`, exactly as
// EnsureControlButtons does for pane 1 (`not_first` = true there).
inline void GiveControlButtons(void* container, void* browserView,
                               bool notFirst) {
  void* bar = NewView(edge155::off::ControlButtons_size);
  using Ctor = void*(__fastcall*)(void*, void*, bool);
  Callable<Ctor>(edge155::rva::ControlButtons_ctor)(bar, browserView, notFirst);
  void* ownedBar = bar;  // stands in for the unique_ptr the callee empties
  ContainerSetControlButtons(container, &ownedBar);
}

inline void* ControlButtonsCloseButton(void* bar) {
  return Field<void*>(bar, edge155::off::ControlButtons_close_button);
}

// One round icon button in the hover bar's style, not yet parented.
inline void* NewControlButton(const void* icon) {
  void* button = NewView(edge155::off::ControlButtons_button_size);
  using CtorFn = void*(__fastcall*)(void*, const void*);
  Callable<CtorFn>(edge155::rva::ControlButtons_Button_ctor)(button, icon);
  return button;
}

// Add a button to a pane's hover bar, exactly as Edge adds ⋯ and X. `icon` is a
// gfx::VectorIcon*; `clickable` = false wires no click handler.
inline void* ControlButtonsAddButton(void* bar, int stringId, const void* icon,
                                     bool clickable) {
  using Fn = void*(__fastcall*)(void*, int, const void*, bool);
  return Callable<Fn>(edge155::rva::ControlButtons_AddButton)(bar, stringId,
                                                              icon, clickable);
}
// Re-run the bar's own layout so its width follows its visible buttons.
inline void ControlButtonsRelayout(void* bar) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::ControlButtons_InitLayout)(bar);
}

// Watch a pane's WebContents for mouse input so hovering it shows its bar.
inline void ListenSplitInput(void* webContents) {
  using Fn = void(__fastcall*)(void*, void*);
  Callable<Fn>(edge155::rva::SplitWindow_ListenWebContentsInputEvent)(
      webContents, nullptr);
}

inline void* SplitWindowControllerFor(void* browser) {
  using Fn = void*(__fastcall*)(void*);
  return Callable<Fn>(edge155::rva::SplitWindowController_FromBrowser)(browser);
}
// Tell the open starting pages of `controller`'s window that `webContents` is
// no longer a tab to offer: each drops its card
// (StartingPageNotifierImpl::OnTabClosing). No notifier yet means no starting
// page has opened, so none to tell.
inline void WithdrawFromStartingPages(void* controller, void* webContents) {
  const void* notifier =
      controller
          ? Field<void*>(controller,
                         edge155::off::SPLITCONTROLLER_starting_page_notifier)
          : nullptr;
  if (!notifier) {
    return;
  }
  using ClosingFn = void(__fastcall*)(const void*, void*);
  Callable<ClosingFn>(edge155::rva::StartingPageNotifierImpl_OnTabClosing)(
      notifier, webContents);
}
// What a pane's hover runs: show the bar of the container showing
// `webContents`, hide the rest.
inline bool SplitWindowControllerShowControlButton(void* controller,
                                                   void* webContents) {
  using Fn = bool(__fastcall*)(void*, void*);
  return Callable<Fn>(edge155::rva::SplitWindowController_ShowControlButton)(
      controller, webContents);
}

// Run the hover's effect for the pane showing `webContents` directly (the test
// channel's stand-in for a real mouse-enter, which needs the real cursor).
inline bool ShowControlButtonFor(void* browser, void* webContents) {
  void* controller = SplitWindowControllerFor(browser);
  if (!controller) {
    return false;
  }
  return SplitWindowControllerShowControlButton(controller, webContents);
}

}  // namespace edgeplus::edge
