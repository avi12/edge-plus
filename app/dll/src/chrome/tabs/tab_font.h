// tab_font.h — the Cyberpunk skin's tab titles in the kit's mono
// (cyberpunk_skin::TitleFontFamilyFor) at the size Edge gave them. A title is a
// views::Label whose font Edge sets once, when the tab is built, so the skin
// sets it as each tab paints: the tab's paint (EdgeTabStyleViews::PaintTab)
// runs for every tab, again whenever the skin is switched (control.h repaints
// the tabs), and a title already in the right font is left alone. Off puts
// back the font Edge gave the titles. UI thread only.
#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include <string_view>
#include "edge/edge.h"
#include "theme/cyberpunk_skin.h"
#include "chrome/tabs/tab_shape.h"

namespace edgeplus::tab_font {

static_assert(cyberpunk_skin::kTitleFontFamily.size() <=
                      edge::ShortString::kCapacity &&
                  cyberpunk_skin::kTitleFontFallback.size() <=
                      edge::ShortString::kCapacity,
              "Edge is handed the family as a short string");

// Edge's own title font, kept from the first title the skin changed: every
// title shares it.
inline constinit std::optional<edge::FontHandle> g_edgeFont;
// The mono at the size it was last made for.
inline constinit std::optional<edge::FontHandle> g_monoFont;
inline constinit int g_monoPixelSize = 0;

inline int CALLBACK NoteFamilyFound(const LOGFONTW* /*font*/,
                                    const TEXTMETRICW* /*metrics*/,
                                    DWORD /*fontType*/, LPARAM found) {
  *reinterpret_cast<bool*>(found) = true;
  return 0;
}

inline bool IsFamilyInstalled(std::string_view family) {
  LOGFONTW query{};
  query.lfCharSet = DEFAULT_CHARSET;
  const std::wstring wideFamily(family.begin(), family.end());
  wcsncpy_s(query.lfFaceName, wideFamily.c_str(), _TRUNCATE);
  bool isFound = false;
  HDC screen = GetDC(nullptr);
  EnumFontFamiliesExW(screen, &query, NoteFamilyFound,
                      reinterpret_cast<LPARAM>(&isFound), 0);
  ReleaseDC(nullptr, screen);
  return isFound;
}

// Looked up once: a font installed later shows after Edge restarts.
inline std::string_view MonoFamily() {
  static const std::string_view kFamily = cyberpunk_skin::TitleFontFamilyFor(
      IsFamilyInstalled(cyberpunk_skin::kTitleFontFamily));
  return kFamily;
}

inline const edge::FontHandle& MonoFontOfSize(int pixelSize) {
  const bool isMade = g_monoFont && g_monoPixelSize == pixelSize;
  if (!isMade) {
    // A short string by the static_assert above.
    g_monoFont =
        edge::MakeFontList(*edge::MakeShortString(MonoFamily()), pixelSize);
    g_monoPixelSize = pixelSize;
  }
  return *g_monoFont;
}

inline void SyncTitleFont(void* tabObject) {
  void* const title = edge::TabTitleLabel(tabObject);
  const edge::FontHandle* const current =
      title ? edge::LabelFontList(title) : nullptr;
  if (!current) {
    return;
  }
  const bool isMono = g_monoFont && current->impl == g_monoFont->impl;
  const bool shouldBeMono = tab_shape::g_isCyberpunkSkinOn;
  if (shouldBeMono && !isMono) {
    if (!g_edgeFont) {
      g_edgeFont = edge::CopyFontList(*current);
    }
    edge::LabelSetFontList(title,
                           MonoFontOfSize(edge::FontListPixelSize(*current)));
    return;
  }
  const bool shouldRestore = !shouldBeMono && isMono && g_edgeFont;
  if (shouldRestore) {
    edge::LabelSetFontList(title, *g_edgeFont);
  }
}

using PaintTab_t = void(__fastcall*)(void*, void*);
inline constinit PaintTab_t o_paint_tab = nullptr;

inline void __fastcall h_paint_tab(void* styleViews, void* canvas) {
  void* const tab = edge::TabObjectOfStyleViews(styleViews);
  if (tab) {
    SyncTitleFont(tab);
  }
  o_paint_tab(styleViews, canvas);
}

}  // namespace edgeplus::tab_font
