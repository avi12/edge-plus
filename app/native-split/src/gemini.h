// gemini.h — the "Gemini" browser assistant. One concern: when the user picks
// Gemini over Copilot, host the Gemini web app in a panel docked at the right
// of the window's content and open/close it from the (reskinned) toolbar chat
// button. The reskin itself is in assistant.h; this file owns only the panel's
// pages and where they sit. What the strip does is share.h's.
//
// Edge ships none of Chrome's glic host (docs/re/gemini-in-chrome.md), so this
// is the regular Gemini web app in the profile's own storage, beside the page,
// not Chrome's in-browser Gemini. It overlays the right of the page rather than
// reflowing it, so it never fights Edge's own content layout. On top of Gemini
// sits a slim strip of our own, a second page that holds the "share a tab"
// buttons (strip_page.h).
#pragma once
#include <windows.h>
#include <map>
#include <string>
#include "edge.h"
#include "glic_library.h"
#include "panel_layout.h"
#include "reserve.h"
#include "share_text.h"
#include "strip_page.h"

namespace fixedge::gemini {

// The Gemini web app, loaded with the profile's own cookies (so a user signed
// in to Google in this Edge is signed in here too).
inline constexpr char kGeminiUrl[] = "https://gemini.google.com/app";

// The same Gemini in its Chrome form: the glic client, which runs once the
// page is offered Chrome's library and a host (glic.h). With no Chrome to take
// the library from, or when Google will not serve glic to this account, the
// panel is the plain web app with the share strip instead.
inline constexpr char kGlicUrl[] = "https://gemini.google.com/glic?hl=en";

enum class PanelMode {
  kGlic,  // Chrome's Gemini UI, hosted by glic.h; no strip
  kApp,   // the Gemini web app, with the share strip
};

// One panel per MultiContentsView (one per browser window): Gemini's page and
// the strip above it, and the parent they are docked in, so a later reposition
// or removal finds them again.
struct Panel {
  void* webView = nullptr;  // Gemini's page
  void* strip = nullptr;    // the share strip, over the top of the panel
  void* parent = nullptr;
  HWND window = nullptr;         // the browser window the panel was built in
  bool isStripExpanded = false;  // the tab picker is open: the strip covers it
  PanelMode mode = PanelMode::kApp;
  ULONGLONG modeSince = 0;  // when this mode began (GetTickCount64)
};

inline bool HasStrip(const Panel& panel) {
  return panel.mode == PanelMode::kApp;
}

inline std::map<void*, Panel>& Panels() {
  static std::map<void*, Panel> panels;
  return panels;
}

// Whether `panel` still belongs to `mcv`: its window exists and still shows
// that MultiContentsView. A closed window destroyed its views, panel pages
// included, so a stale entry must never be touched, and a new window may even
// reuse the freed address.
inline bool IsLive(void* mcv, const Panel& panel) {
  return IsWindow(panel.window) && edge::McvForWindow(panel.window) == mcv;
}

// Forget the panels of closed windows, without touching their pointers.
inline void DropStale() {
  for (auto entry = Panels().begin(); entry != Panels().end();) {
    if (IsLive(entry->first, entry->second)) {
      ++entry;
    } else {
      reserve::Set(entry->first, 0);
      entry = Panels().erase(entry);
    }
  }
}

inline Panel* PanelFor(void* mcv) {
  DropStale();
  const auto entry = Panels().find(mcv);
  return entry == Panels().end() ? nullptr : &entry->second;
}

inline bool IsOpen(const Panel& panel) {
  return edge::ViewGetVisible(panel.webView);
}

// Place the panel's two pages in the content area the MCV occupies.
inline void Layout(void* mcv, const Panel& panel) {
  const panel_layout::Layout layout = panel_layout::PanelLayout(
      edge::ViewBounds(mcv), panel.isStripExpanded, HasStrip(panel));
  edge::SetViewBounds(panel.webView, layout.page);
  edge::SetViewBounds(panel.strip, layout.strip);
  reserve::Set(mcv, layout.page.width);
}

// Lay the page out again beside the panel, or across the whole window once it
// is gone.
inline void RelayoutPage(void* mcv) {
  edge::InvalidateLayout(mcv);
  edge::LayoutImmediately(mcv);
}

// Keep an open panel glued to the right edge after a window resize or a layout
// pass (called from the layout detour and WM_SIZE, both on the UI thread).
inline void Reposition(void* mcv) {
  const Panel* panel = PanelFor(mcv);
  if (panel && IsOpen(*panel)) {
    Layout(mcv, *panel);
  }
}

// The picker opened or closed: the strip grows to cover the panel, or shrinks
// back to its bar.
inline void SetStripExpanded(void* mcv, bool isExpanded) {
  Panel* panel = PanelFor(mcv);
  if (panel && panel->isStripExpanded != isExpanded) {
    panel->isStripExpanded = isExpanded;
    Reposition(mcv);
  }
}

// Build the panel for `mcv` on first use: a WebView on the window's profile,
// docked as the last child of the MCV's parent (so it draws over the page),
// loading Gemini, and the strip over it. Returns null if the profile can't be
// found.
inline Panel* EnsurePanel(HWND window, void* mcv) {
  if (Panel* existing = PanelFor(mcv)) {
    return existing;
  }
  void* tsm = edge::McvTabStripModel(mcv);
  void* profile = tsm ? edge::ProfileForTabStrip(tsm) : nullptr;
  void* parent = edge::ViewParent(mcv);
  if (!profile || !parent) {
    return nullptr;
  }
  void* webView = edge::NewWebView(profile);
  edge::SetViewVisible(webView, false);
  edge::AddChildViewAt(parent, webView, edge::ViewChildCount(parent));
  const PanelMode mode =
      glic_library::Load().empty() ? PanelMode::kApp : PanelMode::kGlic;
  edge::WebViewLoadUrl(webView,
                       mode == PanelMode::kGlic ? kGlicUrl : kGeminiUrl);
  void* strip = edge::NewWebView(profile);
  edge::SetViewVisible(strip, false);
  edge::AddChildViewAt(parent, strip, edge::ViewChildCount(parent));
  const std::string stripUrl = share_text::BuildDataUrl(
      std::string(strip_page::kHtmlStyle) + strip_page::kHtmlBody);
  edge::WebViewLoadUrl(strip, stripUrl.c_str());
  Panel& panel = Panels()[mcv];
  panel.webView = webView;
  panel.strip = strip;
  panel.parent = parent;
  panel.window = window;
  panel.mode = mode;
  panel.modeSince = GetTickCount64();
  Log("[gemini] panel created webView=%p strip=%p parent=%p", webView, strip,
      parent);
  return &panel;
}

// Returns whether the panel is now open (the toolbar button's toggle).
inline bool Toggle(HWND window, void* mcv) {
  const Panel* panel = EnsurePanel(window, mcv);
  if (!panel) {
    return false;
  }
  const bool isOpening = !IsOpen(*panel);
  edge::SetViewVisible(panel->webView, isOpening);
  edge::SetViewVisible(panel->strip, isOpening && HasStrip(*panel));
  if (isOpening) {
    Layout(mcv, *panel);
    edge::ViewRequestFocus(panel->webView);
  } else {
    reserve::Set(mcv, 0);
  }
  RelayoutPage(mcv);
  Log("[gemini] panel %s", isOpening ? "opened" : "closed");
  return isOpening;
}

// Give up on glic for this panel (Google would not serve it, or the client
// never started): load the plain web app in the same page and show the strip.
inline void SwitchToApp(void* mcv) {
  Panel* panel = PanelFor(mcv);
  if (!panel || panel->mode == PanelMode::kApp) {
    return;
  }
  panel->mode = PanelMode::kApp;
  panel->modeSince = GetTickCount64();
  edge::WebViewLoadUrl(panel->webView, kGeminiUrl);
  edge::SetViewVisible(panel->strip, IsOpen(*panel));
  Reposition(mcv);
  Log("[gemini] glic is not available here: showing the web app instead");
}

// Tear a window's panel down (on unload, or when the user switches back to
// Copilot): remove it from the view tree and destroy it with its WebContents.
inline void Remove(void* mcv) {
  const Panel* panel = PanelFor(mcv);
  if (!panel) {
    return;
  }
  reserve::Set(mcv, 0);
  edge::ViewRemoveChild(panel->parent, panel->strip);
  edge::DeleteWebView(panel->strip);
  edge::ViewRemoveChild(panel->parent, panel->webView);
  edge::DeleteWebView(panel->webView);
  Panels().erase(mcv);
  RelayoutPage(mcv);
}

inline void RemoveAll() {
  DropStale();
  while (!Panels().empty()) {
    Remove(Panels().begin()->first);
  }
}

}  // namespace fixedge::gemini
