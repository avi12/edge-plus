// navigation.h — a page's navigation: its opener, loading a URL in it,
// opening one in a background tab, and a split's starting page.
#pragma once
#include <cstdint>
#include <cstring>
#include "edge/api/binding.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// Whether `webContents` was opened by another page that can still script it.
inline bool HasOpener(void* webContents) {
  using OpenerFn = bool(__fastcall*)(void*);
  return Callable<OpenerFn>(edge155::rva::WebContentsImpl_HasOpener)(
      webContents);
}

// Load `url` (a GURL*) in `webContents`, as a link would.
inline void NavigateTo(void* webContents, const void* url) {
  alignas(16) uint8_t params[edge155::off::LoadURLParams_size] = {};
  using ParamsFn = void*(__fastcall*)(void*, const void*);
  Callable<ParamsFn>(edge155::rva::LoadURLParams_ctor)(params, url);
  using ControllerFn = const void*(__fastcall*)(void*);
  const void* controller = Callable<ControllerFn>(
      edge155::rva::WebContentsImpl_GetController)(webContents);
  alignas(8) uint8_t navigationHandle[edge155::off::WeakPtr_size] = {};
  using LoadFn = void(__fastcall*)(const void*, void*, const void*);
  Callable<LoadFn>(edge155::rva::NavigationControllerImpl_LoadURLWithParams)(
      controller, navigationHandle, params);
  using DtorFn = void(__fastcall*)(void*);
  Callable<DtorFn>(edge155::rva::WeakReference_dtor)(navigationHandle);
  Callable<DtorFn>(edge155::rva::LoadURLParams_dtor)(params);
}

// Open the GURL `gurl` in a new background tab at strip `index`
// (chrome::AddTabAt's worker); returns its WebContents, or null. `browser` is
// what BrowserOf returns: the BrowserWindowInterface view of Edge's Browser
// (tab_strip_model at +0x30), as AddAndReturnTabAt takes it.
inline void* AddBackgroundTabForGurl(void* browser, const void* gurl,
                                     int index) {
  alignas(8) uint8_t noGroup[edge155::off::OptionalTabGroupId_size] = {};
  constexpr uint32_t kNoExtraTransition = 0;
  constexpr bool kIsForeground = false;
  constexpr bool kIsPinned = false;
  constexpr uint64_t kNoOptionalInt = 0;  // an empty optional<int>
  using AddFn = void*(__fastcall*)(void*, const void*, uint32_t, int, bool,
                                   const void*, bool, uint64_t);
  return Callable<AddFn>(edge155::rva::Chrome_AddAndReturnTabAt)(
      browser, gurl, kNoExtraTransition, index, kIsForeground, noGroup,
      kIsPinned, kNoOptionalInt);
}
inline void* AddBackgroundTab(void* browser, const char* url, int index) {
  const struct {
    const char* data;
    size_t size;
  } urlView = {url, std::strlen(url)};
  alignas(16) uint8_t gurl[edge155::off::GURL_size] = {};
  using GurlFn = void*(__fastcall*)(void*, const void*);
  Callable<GurlFn>(edge155::rva::GURL_ctor_string)(gurl, &urlView);
  void* webContents = AddBackgroundTabForGurl(browser, gurl, index);
  using DtorFn = void(__fastcall*)(void*);
  Callable<DtorFn>(edge155::rva::GURL_dtor)(gurl);
  return webContents;
}

// The page Edge's split button opens in its new pane: it offers the window's
// other tabs.
inline const char* StartingPageUrl() {
  return static_cast<const char*>(
      At(edge155::rva::EdgeUISplitWindowStartingPageURL));
}

// Whether `webContents` is Edge's split starting page, judged as Edge's split
// controller judges it: by the user data the page attaches to its contents.
inline bool IsStartingPage(void* webContents) {
  constexpr bool kIsJudgedByUrl = false;
  using StartingFn = bool(__fastcall*)(void*, bool);
  return Callable<StartingFn>(
      edge155::rva::SplitWindowController_IsStartingPage)(webContents,
                                                          kIsJudgedByUrl);
}

// A task's bound base::WeakPtr (its BindState's first bound argument): its
// pointer while the WeakPtr is valid, else null.
inline void* BoundWeakTarget(void* bindState) {
  void* flag =
      Field<void*>(bindState, edge155::off::BINDSTATE_first_bound_argument);
  const bool isValid =
      (flag != nullptr) &&
      Field<uint8_t>(flag, edge155::off::WEAKFLAG_invalidated) == 0;
  return isValid ? Field<void*>(bindState,
                                edge155::off::BINDSTATE_first_bound_argument +
                                    sizeof(void*))
                 : nullptr;
}
// Null the bound WeakPtr's pointer, so the task finds nothing to act on and
// only releases its reference.
inline void ClearBoundWeakTarget(void* bindState) {
  Field<void*>(bindState, edge155::off::BINDSTATE_first_bound_argument +
                              sizeof(void*)) = nullptr;
}

// The WebContents of the starting page a SplitWindowStartingUIHandler serves.
inline void* StartingPageOf(void* handler) {
  void* webUi = Field<void*>(handler, edge155::off::STARTINGHANDLER_web_ui);
  return webUi ? Field<void*>(webUi, edge155::off::WEBUI_web_contents)
               : nullptr;
}

}  // namespace edgeplus::edge
