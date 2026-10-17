// gemini.h — the "Gemini" browser assistant. One concern: when the user picks
// Gemini over Copilot, host the Gemini web app in a panel docked at the right
// of the window's content and open/close it from the (reskinned) toolbar chat
// button. The reskin itself is in assistant.h; this file owns only the panel.
//
// Edge ships none of Chrome's glic host (docs/re/gemini-in-chrome.md), so this
// is the regular Gemini web app in the profile's own storage, beside the page,
// not Chrome's in-browser Gemini. It overlays the right of the page rather than
// reflowing it, so it never fights Edge's own content layout.
#pragma once
#include <map>
#include "edge.h"

namespace fixedge::gemini {

// The Gemini web app, loaded with the profile's own cookies (so a user signed
// in to Google in this Edge is signed in here too).
inline constexpr char kGeminiUrl[] = "https://gemini.google.com/app";

// The docked panel's width in DIPs, and the gap it leaves at the content's top
// (so it sits below the pane's own chrome, like Edge's side pane).
inline constexpr int kPanelWidth = 420;
inline constexpr int kPanelTopInset = 0;

// One panel per MultiContentsView (one per browser window): the WebView and the
// parent it is docked in, so a later reposition or removal finds them again.
struct Panel {
  void* webView = nullptr;
  void* parent = nullptr;
};

inline std::map<void*, Panel>& Panels() {
  static std::map<void*, Panel> panels;
  return panels;
}

inline const Panel* PanelFor(void* mcv) {
  const auto entry = Panels().find(mcv);
  return entry == Panels().end() ? nullptr : &entry->second;
}

// The panel's bounds in its parent's coordinates: the right kPanelWidth of the
// content area the MCV occupies.
inline Rect PanelBounds(void* mcv) {
  const Rect content = edge::ViewBounds(mcv);
  const int width = kPanelWidth < content.width ? kPanelWidth : content.width;
  return Rect{content.x + content.width - width, content.y + kPanelTopInset,
              width, content.height - kPanelTopInset};
}

// Keep an open panel glued to the right edge after a window resize or a layout
// pass (called from the layout detour and WM_SIZE, both on the UI thread).
inline void Reposition(void* mcv) {
  const Panel* panel = PanelFor(mcv);
  if (panel && edge::ViewGetVisible(panel->webView)) {
    edge::SetViewBounds(panel->webView, PanelBounds(mcv));
  }
}

// Build the panel for `mcv` on first use: a WebView on the window's profile,
// docked as the last child of the MCV's parent (so it draws over the page),
// loading Gemini. Returns null if the profile can't be found.
inline const Panel* EnsurePanel(void* mcv) {
  if (const Panel* existing = PanelFor(mcv)) {
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
  edge::WebViewLoadUrl(webView, kGeminiUrl);
  Panel& panel = Panels()[mcv];
  panel.webView = webView;
  panel.parent = parent;
  Log("[gemini] panel created webView=%p parent=%p", webView, parent);
  return &panel;
}

// Returns whether the panel is now open (the toolbar button's toggle).
inline bool Toggle(void* mcv) {
  const Panel* panel = EnsurePanel(mcv);
  if (!panel) {
    return false;
  }
  const bool isOpening = !edge::ViewGetVisible(panel->webView);
  edge::SetViewVisible(panel->webView, isOpening);
  if (isOpening) {
    edge::SetViewBounds(panel->webView, PanelBounds(mcv));
    edge::ViewRequestFocus(panel->webView);
  }
  Log("[gemini] panel %s", isOpening ? "opened" : "closed");
  return isOpening;
}

// Tear a window's panel down (on unload, or when the user switches back to
// Copilot): remove it from the view tree and destroy it with its WebContents.
inline void Remove(void* mcv) {
  const Panel* panel = PanelFor(mcv);
  if (!panel) {
    return;
  }
  edge::ViewRemoveChild(panel->parent, panel->webView);
  edge::DeleteWebView(panel->webView);
  Panels().erase(mcv);
}

inline void RemoveAll() {
  while (!Panels().empty()) {
    Remove(Panels().begin()->first);
  }
}

}  // namespace fixedge::gemini
