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
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "anim.h"
#include "chat_action.h"
#include "edge.h"
#include "glic_library.h"
#include "motion.h"
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
  // The slide: the panel eases between hidden (0) and fully out (1) like
  // Copilot's sidebar. `isWanted` is where it is heading; it starts from
  // `slideFrom` at `slideStart`. The views stay visible until a close lands.
  bool isWanted = false;
  double slideFrom = 0;
  ULONGLONG slideStart = 0;
  PanelMode mode = PanelMode::kApp;
  // What the chat menu asked for before the page was ready to do it (or before
  // glic gave way to the web app); share.h and glic.h carry it out.
  chat_action::ChatAction pendingAction = chat_action::ChatAction::kNone;
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

// Open as far as the user is concerned: it is, or is sliding out. A closing
// panel is already closed to everything else.
inline bool IsOpen(const Panel& panel) {
  return panel.isWanted;
}

// Told, after a panel opens or closes, so the toolbar button can follow (its
// icon is the open state's). Set by assistant.h.
inline void (*g_onOpenChanged)(void* mcv) = nullptr;

// On screen at all, which a closing panel still is until its slide lands.
// Whether any window's panel is open.
inline bool IsAnyOpen() {
  for (const auto& [mcv, panel] : Panels()) {
    if (IsOpen(panel)) {
      return true;
    }
  }
  return false;
}

inline bool IsShown(const Panel& panel) {
  return edge::ViewGetVisible(panel.webView);
}

inline constexpr UINT_PTR kTimerId = 0xF1F0;
inline constexpr UINT kFrameMs = 15;

// How far out the panel is now, 0..1.
inline double SlideFraction(const Panel& panel) {
  const double target = panel.isWanted ? 1.0 : 0.0;
  if (!anim::g_isEnabled) {
    return target;
  }
  const double progress =
      static_cast<double>(GetTickCount64() - panel.slideStart) /
      motion::kDurationMs;
  return panel.slideFrom + (target - panel.slideFrom) * motion::Ease(progress);
}

inline bool IsSliding(const Panel& panel) {
  const double target = panel.isWanted ? 1.0 : 0.0;
  return SlideFraction(panel) != target;
}

// Place the panel's two pages in the content area the MCV occupies, slid in
// from the right edge by how far out it is.
inline void Layout(void* mcv, const Panel& panel) {
  const double fraction = SlideFraction(panel);
  panel_layout::Layout layout = panel_layout::PanelLayout(
      edge::ViewBounds(mcv), panel.isStripExpanded, HasStrip(panel));
  const int hidden =
      static_cast<int>(std::lround(layout.page.width * (1.0 - fraction)));
  layout.page.x += hidden;
  layout.strip.x += hidden;
  edge::SetViewBounds(panel.webView, layout.page);
  edge::SetViewBounds(panel.strip, layout.strip);
  reserve::Set(mcv, layout.page.width - hidden);
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
  if (panel && IsShown(*panel)) {
    Layout(mcv, *panel);
  }
}

// WM_TIMER of the slide: lay out every sliding panel of `window` again, and
// once a close has landed hide its pages. The timer stops when none slides.
inline void Tick(HWND window) {
  DropStale();
  bool isAnySliding = false;
  std::vector<void*> landed;
  for (auto& [mcv, panel] : Panels()) {
    if (panel.window != window || !IsShown(panel)) {
      continue;
    }
    if (IsSliding(panel)) {
      isAnySliding = true;
    } else if (!panel.isWanted) {
      landed.push_back(mcv);
    }
    edge::InvalidateLayout(mcv);
    edge::LayoutImmediately(mcv);
  }
  for (void* mcv : landed) {
    const Panel& panel = Panels().at(mcv);
    edge::SetViewVisible(panel.webView, false);
    edge::SetViewVisible(panel.strip, false);
    reserve::Set(mcv, 0);
    RelayoutPage(mcv);
  }
  if (!isAnySliding) {
    KillTimer(window, kTimerId);
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
  const std::string stripUrl =
      share_text::BuildDataUrl(strip_page::HtmlStyle() + strip_page::kHtmlBody);
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
  Panel* panel = EnsurePanel(window, mcv);
  if (!panel) {
    return false;
  }
  const bool isOpening = !IsOpen(*panel);
  panel->slideFrom = IsShown(*panel) ? SlideFraction(*panel) : 0.0;
  panel->slideStart = GetTickCount64();
  panel->isWanted = isOpening;
  if (isOpening) {
    edge::SetViewVisible(panel->webView, true);
    edge::SetViewVisible(panel->strip, HasStrip(*panel));
    edge::ViewRequestFocus(panel->webView);
  }
  SetTimer(panel->window, kTimerId, kFrameMs, nullptr);
  Layout(mcv, *panel);
  RelayoutPage(mcv);
  if (g_onOpenChanged) {
    g_onOpenChanged(mcv);
  }
  Log("[gemini] panel %s", isOpening ? "opened" : "closed");
  return isOpening;
}

// Open the window's panel if it is closed (the chat menu's items always open
// it; the toolbar button toggles). False if there is no panel to open.
inline bool EnsureOpen(HWND window, void* mcv) {
  const Panel* panel = EnsurePanel(window, mcv);
  if (!panel) {
    return false;
  }
  return IsOpen(*panel) || Toggle(window, mcv);
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
  KillTimer(panel->window, kTimerId);
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
