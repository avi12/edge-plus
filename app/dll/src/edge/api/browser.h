// browser.h — a browser window: from its HWND to its BrowserView, Browser,
// TabStripModel, MultiContentsView and Profile.
#pragma once
#include "edge/api/binding.h"
#include "edge/api/tab_strip.h"
#include "edge/api/web_contents.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// HWND -> aura::Window* -> BrowserView*, the bridge to reach a browser from a
// Win32 window handle (Chromium is Aura; GetBrowserViewForNativeWindow wants the
// aura window, not the HWND).
inline void* AuraWindowForHWND(void* hwnd) {
  using Fn = void*(__fastcall*)(void*);
  return Callable<Fn>(edge155::rva::GetContentWindowForHWND)(hwnd);
}
inline void* BrowserViewForAura(void* auraWindow) {
  using Fn = void*(__fastcall*)(void*);
  return Callable<Fn>(edge155::rva::BrowserView_ForNativeWindow)(auraWindow);
}

inline void* BrowserViewMcv(void* browserView) {
  return Field<void*>(browserView, edge155::off::BV_multi_contents_view);
}

inline void* BrowserOf(void* browserView) {
  return Field<void*>(browserView, edge155::off::BV_browser);
}
inline void* TabStripModelOf(void* browser) {
  return Field<void*>(browser, edge155::off::BROWSER_tab_strip_model);
}

inline void* TsmForWindow(void* hwnd) {
  void* auraWindow = AuraWindowForHWND(hwnd);
  if (!auraWindow) {
    return nullptr;
  }
  void* browserView = BrowserViewForAura(auraWindow);
  return browserView ? TabStripModelOf(BrowserOf(browserView)) : nullptr;
}

inline void* McvForWindow(void* hwnd) {
  void* auraWindow = AuraWindowForHWND(hwnd);
  if (!auraWindow) {
    return nullptr;
  }
  void* browserView = BrowserViewForAura(auraWindow);
  return browserView ? BrowserViewMcv(browserView) : nullptr;
}

// The Profile (BrowserContext) behind a TabStripModel's active tab, our bridge
// to that profile's ThemeService and ExtensionRegistry.
inline void* ProfileForTabStrip(void* tsm) {
  void* tab = tsm ? ActiveTab(tsm) : nullptr;
  void* webContents = tab ? TabContents(tab) : nullptr;
  return webContents ? BrowserContextOf(webContents) : nullptr;
}

}  // namespace edgeplus::edge
