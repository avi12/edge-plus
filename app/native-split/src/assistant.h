// assistant.h — the browser-assistant switch. One concern: when the user picks
// Gemini (config.h), reskin Edge's toolbar chat button as Gemini and open the
// Gemini panel (gemini.h) instead of Copilot's pane. The button's behaviour
// object supplies its image, tooltip, accessible name and click, so hooking
// those (plus the button's label) reskins and reroutes it without touching the
// button class. Every hook runs the original untouched while Copilot is picked.
#pragma once
#include <string>
#include <vector>
#include "MinHook.h"
#include "channel.h"
#include "config.h"
#include "edge.h"
#include "gemini.h"
#include "icons.h"

namespace fixedge::assistant {

using namespace fixedge::edge155;

// Which assistant the chat button opens; the panel pushes it (config.h).
inline config::Assistant g_assistant = config::Assistant::kCopilot;

inline bool IsGemini() {
  return g_assistant == config::Assistant::kGemini;
}

// Gemini's own name, for the tooltip, accessible name and label (the one place
// the reskin's visible text is written).
inline constexpr char kGeminiName[] = "Gemini";
inline const char16_t kGeminiNameUtf16[] = u"Gemini";

// The spark is drawn at the toolbar icon size, in the button's foreground
// colour. The id is Edge's own toolbar-foreground ColorId.
inline constexpr int kIconDipSize = 16;
inline constexpr uint32_t kToolbarForegroundColorId = 0xcf6;

// --- the action's image / tooltip / accessible-name / click ------------------

using Image_t = void*(__fastcall*)(void*, void*, void*, void*);
inline Image_t o_normal_image = nullptr;
inline Image_t o_pressed_image = nullptr;

// Draw the Gemini spark into the ImageSkia out-param, in the button's colour
// from the color provider, instead of Copilot's bitmap logo.
inline void* DrawGeminiSpark(void* out, void* colorProvider) {
  uint32_t color = 0xff000000;
  if (colorProvider) {
    using GetColorFn = uint32_t(__fastcall*)(void*, uint32_t);
    color = reinterpret_cast<GetColorFn>(edge::At(rva::ColorProvider_GetColor))(
        colorProvider, kToolbarForegroundColorId);
  }
  edge::CreateVectorIcon(out, icons::GeminiSpark(), kIconDipSize, color);
  return out;
}

inline void* __fastcall h_normal_image(void* action, void* out,
                                       void* colorProvider, void* nativeTheme) {
  if (IsGemini()) {
    return DrawGeminiSpark(out, colorProvider);
  }
  return o_normal_image(action, out, colorProvider, nativeTheme);
}
inline void* __fastcall h_pressed_image(void* action, void* out,
                                        void* colorProvider,
                                        void* nativeTheme) {
  if (IsGemini()) {
    return DrawGeminiSpark(out, colorProvider);
  }
  return o_pressed_image(action, out, colorProvider, nativeTheme);
}

using Text_t = void*(__fastcall*)(void*, void*);
inline Text_t o_tooltip = nullptr;
inline Text_t o_accessible_name = nullptr;

inline void* FillGeminiName(void* out) {
  const edge::U8View name{kGeminiName, sizeof(kGeminiName) - 1};
  edge::Utf8ToUtf16(reinterpret_cast<edge::U16String*>(out), &name);
  return out;
}
inline void* __fastcall h_tooltip(void* action, void* out) {
  if (IsGemini()) {
    return FillGeminiName(out);
  }
  return o_tooltip(action, out);
}
inline void* __fastcall h_accessible_name(void* action, void* out) {
  if (IsGemini()) {
    return FillGeminiName(out);
  }
  return o_accessible_name(action, out);
}

using OnClick_t = void(__fastcall*)(void*);
inline OnClick_t o_on_click = nullptr;

// The browser window the click is in (the foreground browser window), so the
// right window's Gemini panel toggles.
inline void* ActiveBrowserMcv() {
  void* mcv = edge::McvForWindow(GetForegroundWindow());
  return mcv ? mcv : edge::McvForWindow(channel::g_window);
}

inline void __fastcall h_on_click(void* action) {
  if (IsGemini()) {
    void* mcv = ActiveBrowserMcv();
    if (mcv) {
      gemini::Toggle(mcv);
    }
    return;
  }
  o_on_click(action);
}

// --- the button's "Chat" label -----------------------------------------------

using Label_t = void(__fastcall*)(void*);
inline Label_t o_update_label = nullptr;

inline void __fastcall h_update_label(void* button) {
  o_update_label(button);
  if (IsGemini()) {
    edge::LabelButtonSetText(
        button, edge::U16View{kGeminiNameUtf16,
                              sizeof(kGeminiNameUtf16) / sizeof(char16_t) - 1});
    edge::ViewSetAccessibleName(button, kGeminiName);
  }
}

// --- re-skin the live button when the choice changes -------------------------

inline int g_viewsVisited = 0;

// Collect every chat button under `view` (one per browser window; usually one).
inline void CollectButtons(void* view, std::vector<void*>* buttons) {
  g_viewsVisited++;
  if (edge::IsCopilotButton(view)) {
    buttons->push_back(view);
  }
  const size_t childCount = edge::ViewChildCount(view);
  for (size_t i = 0; i < childCount; i++) {
    CollectButtons(edge::ViewChildAt(view, i), buttons);
  }
}

// The accessible name the stock button shows for its label: the label itself,
// as UTF-8 (a label is a few ASCII-range words).
inline std::string LabelAsUtf8(const edge::U16View label) {
  std::string utf8;
  for (size_t i = 0; i < label.length; i++) {
    utf8.push_back(static_cast<char>(label.data[i]));
  }
  return utf8;
}

// Put the stock label back on `button` after the reskin replaced it. Edge's own
// label update does not always run to the end (without its label controller it
// returns at once) and, when it does run, skips the write when the text equals
// its cache, so it cannot be relied on to undo the reskin. The cache still
// holds the stock text the reskin never touched: write that, and make it the
// accessible name again.
inline void RestoreStockLabel(void* button) {
  const edge::U16View stockLabel = edge::CopilotButtonCachedLabel(button);
  const bool hasStockLabel = stockLabel.length > 0;
  if (!hasStockLabel) {
    return;
  }
  edge::LabelButtonSetText(button, stockLabel);
  edge::ViewSetAccessibleName(button, LabelAsUtf8(stockLabel).c_str());
}

// Re-skin (or restore) each live chat button now: its images and its label are
// cached, so force both to be rebuilt through our hooks. `mcv` names one
// window's view tree. `isRestoring` is switching back from Gemini.
inline void Refresh(void* mcv, bool isRestoring) {
  if (!mcv) {
    return;
  }
  std::vector<void*> buttons;
  g_viewsVisited = 0;
  CollectButtons(edge::ViewRoot(mcv), &buttons);
  Log("[assistant] walked %d views", g_viewsVisited);
  for (void* button : buttons) {
    edge::CopilotButtonRefreshImages(button);
    edge::CopilotButtonUpdateLabel(button);
    if (isRestoring) {
      RestoreStockLabel(button);
    }
  }
  Log("[assistant] refreshed %zu chat button(s), gemini=%d", buttons.size(),
      static_cast<int>(IsGemini()));
}

// --- install / apply ---------------------------------------------------------

// The assistant hooks are independent of the split: keep them live whenever the
// DLL is loaded (they are gated on g_assistant, so Copilot is pass-through), so
// turning the split off with MH_ALL_HOOKS does not un-reskin the button.
inline bool EnableAll() {
  return MH_EnableHook(edge::At(rva::CopilotAction_GetNormalStateImage)) ==
             MH_OK &&
         MH_EnableHook(edge::At(rva::CopilotAction_GetPressedStateImage)) ==
             MH_OK &&
         MH_EnableHook(edge::At(rva::CopilotAction_GetButtonTooltipText)) ==
             MH_OK &&
         MH_EnableHook(edge::At(rva::CopilotAction_GetAccessibleName)) ==
             MH_OK &&
         MH_EnableHook(edge::At(rva::CopilotAction_OnClick)) == MH_OK &&
         MH_EnableHook(
             edge::At(rva::CopilotButton_UpdateLabelFromController)) == MH_OK;
}

inline bool Install() {
  const bool isHooked =
      MH_CreateHook(edge::At(rva::CopilotAction_GetNormalStateImage),
                    reinterpret_cast<void*>(h_normal_image),
                    reinterpret_cast<void**>(&o_normal_image)) == MH_OK &&
      MH_CreateHook(edge::At(rva::CopilotAction_GetPressedStateImage),
                    reinterpret_cast<void*>(h_pressed_image),
                    reinterpret_cast<void**>(&o_pressed_image)) == MH_OK &&
      MH_CreateHook(edge::At(rva::CopilotAction_GetButtonTooltipText),
                    reinterpret_cast<void*>(h_tooltip),
                    reinterpret_cast<void**>(&o_tooltip)) == MH_OK &&
      MH_CreateHook(edge::At(rva::CopilotAction_GetAccessibleName),
                    reinterpret_cast<void*>(h_accessible_name),
                    reinterpret_cast<void**>(&o_accessible_name)) == MH_OK &&
      MH_CreateHook(edge::At(rva::CopilotAction_OnClick),
                    reinterpret_cast<void*>(h_on_click),
                    reinterpret_cast<void**>(&o_on_click)) == MH_OK &&
      MH_CreateHook(edge::At(rva::CopilotButton_UpdateLabelFromController),
                    reinterpret_cast<void*>(h_update_label),
                    reinterpret_cast<void**>(&o_update_label)) == MH_OK;
  if (!isHooked) {
    Log("[err] assistant hooks failed");
    return false;
  }
  const bool isEnabled = EnableAll();
  Log("[ok] assistant hooks installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

// Apply the panel's choice: record it, and reskin/restore the live button of
// `mcv`'s window. Switching back to Copilot also closes any open Gemini panel.
inline void Apply(void* mcv, config::Assistant assistant) {
  const bool wasGemini = IsGemini();
  g_assistant = assistant;
  const bool isRestoring = wasGemini && !IsGemini();
  if (isRestoring) {
    gemini::Remove(mcv);
  }
  Refresh(mcv, isRestoring);
}

}  // namespace fixedge::assistant
