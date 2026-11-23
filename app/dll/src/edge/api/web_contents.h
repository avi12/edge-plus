// web_contents.h — a tab's page (content::WebContents) and its frames: every
// open page, its URLs and profile, whether it is live, and showing and
// reloading it.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "edge/api/binding.h"
#include "edge/api/strings.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// Force a (background) WebContents to the visible state so the pane paints.
inline void WebContentsWasShown(void* webContents) {
  using Fn = void*(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::WebContents_WasShown)(webContents);
}

// The URL of the page `webContents` last committed (a const GURL&): the page
// Edge's split "Open this webpage in a new tab" copies.
inline const void* LastCommittedUrl(void* webContents) {
  using UrlFn = const void*(__fastcall*)(void*);
  return Callable<UrlFn>(edge155::rva::WebContentsImpl_GetLastCommittedURL)(
      webContents);
}

// content::BrowserContext* (the Profile) behind a WebContents.
inline void* BrowserContextOf(void* webContents) {
  using Fn = void*(__fastcall*)(void*);
  return Callable<Fn>(edge155::rva::WebContentsImpl_GetBrowserContext)(
      webContents);
}

// Reload the page in `webContents` as the reload button does (no repost
// prompt); `reloadType` is a content::ReloadType (off::RELOADTYPE_*).
inline void ReloadPage(void* webContents,
                       int32_t reloadType = edge155::off::RELOADTYPE_normal) {
  using Fn = void(__fastcall*)(void*, int32_t, bool);
  Callable<Fn>(edge155::rva::NavigationControllerImpl_Reload)(
      reinterpret_cast<uint8_t*>(webContents) +
          edge155::off::WEBCONTENTS_controller,
      reloadType, false);
}

// Every WebContents of the browser process (tabs, side panels, hidden pages).
inline std::vector<void*> AllWebContents() {
  struct Listing {
    void** data = nullptr;
    uint64_t count = 0;
    void* unused = nullptr;
  } all{};
  using Fn = void*(__fastcall*)(Listing*);
  Callable<Fn>(edge155::rva::WebContentsImpl_GetAllWebContents)(&all);
  return all.data ? std::vector<void*>(all.data, all.data + all.count)
                  : std::vector<void*>();
}

// The last committed URL of `webContents`, in UTF-8.
inline std::string TabUrl(void* webContents) {
  using SpecFn = const void*(__fastcall*)(const void*);
  const void* url = LastCommittedUrl(webContents);
  const void* spec =
      url ? Callable<SpecFn>(edge155::rva::GURL_spec)(url) : nullptr;
  return spec ? StdStringText(spec) : std::string();
}

// The URL `webContents` shows (the pending one during a navigation), in UTF-8.
inline std::string VisibleUrl(void* webContents) {
  using UrlFn = const void*(__fastcall*)(void*);
  void* const* vtable = *reinterpret_cast<void***>(webContents);
  const void* url = reinterpret_cast<UrlFn>(
      vtable[edge155::off::WEBCONTENTS_get_visible_url_slot / sizeof(void*)])(
      webContents);
  using SpecFn = const void*(__fastcall*)(const void*);
  const void* spec =
      url ? Callable<SpecFn>(edge155::rva::GURL_spec)(url) : nullptr;
  return spec ? StdStringText(spec) : std::string();
}

inline void* PrimaryMainFrame(void* webContents) {
  using Fn = void*(__fastcall*)(void*);
  return Callable<Fn>(edge155::rva::WebContentsImpl_GetPrimaryMainFrame)(
      webContents);
}

// Whether the tab's page has a running renderer: false for a discarded
// (asleep) tab, whose page cannot run a script.
inline bool IsPageLive(void* webContents) {
  const void* frame = PrimaryMainFrame(webContents);
  using Fn = bool(__fastcall*)(const void*);
  return (frame != nullptr) &&
         Callable<Fn>(edge155::rva::RenderFrameHostImpl_IsRenderFrameLive)(
             frame);
}

// The page's main frame, or null when the page is not live.
inline const void* LiveMainFrame(void* webContents) {
  return IsPageLive(webContents) ? PrimaryMainFrame(webContents) : nullptr;
}

// Every frame of a page (the main frame first, then its iframes).
inline std::vector<void*> FramesOf(void* webContents) {
  struct FunctionRef {
    void* object = nullptr;
    void* invoker = nullptr;
  };
  std::vector<void*> frames;
  const FunctionRef collector{
      &frames, reinterpret_cast<void*>(+[](void* object, void* frame) {
        static_cast<std::vector<void*>*>(object)->push_back(frame);
      })};
  using Fn = void(__fastcall*)(void*, const FunctionRef*);
  Callable<Fn>(edge155::rva::WebContentsImpl_ForEachRenderFrameHost)(
      webContents, &collector);
  return frames;
}

inline std::string FrameUrl(void* frame) {
  using UrlFn = const void*(__fastcall*)(void*);
  using SpecFn = const void*(__fastcall*)(const void*);
  const void* url = Callable<UrlFn>(
      edge155::rva::RenderFrameHostImpl_GetLastCommittedURL)(frame);
  const void* spec =
      url ? Callable<SpecFn>(edge155::rva::GURL_spec)(url) : nullptr;
  return spec ? StdStringText(spec) : std::string();
}

}  // namespace edgeplus::edge
