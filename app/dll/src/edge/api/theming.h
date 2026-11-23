// theming.h — Edge's themes: a profile's ThemeService, a theme extension
// enabled, reinstalled or applied, the default theme, a theme's palette and
// tone, the reading view's theme, and telling a page its colours changed.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/api/browser.h"
#include "edge/offsets.h"
#include "theme/theme_tone.h"

namespace edgeplus::edge {

// Set the reading mode's page theme of the profile behind `browserContext`
// (dom_distiller's pref, `theme` its number): persisted, and every open reading
// view of the profile switches to it.
inline void SetReadingTheme(void* browserContext, int32_t theme) {
  using GetFn = void*(__fastcall*)(void*);
  void* keyed = Callable<GetFn>(
      edge155::rva::DomDistillerServiceFactory_GetForBrowserContext)(
      browserContext);
  if (!keyed) {
    return;
  }
  void* service =
      static_cast<uint8_t*>(keyed) + edge155::off::DOMDISTILLERKEYED_service;
  const void* prefs = Field<void*>(
      service, edge155::off::DOMDISTILLERSERVICE_distilled_page_prefs);
  if (!prefs) {
    return;
  }
  // An empty libc++ std::string (short, length 0): the callee owns and frees it,
  // and frees nothing for a short one.
  struct EmptyString {
    uint64_t words[3] = {};
  };
  static_assert(sizeof(EmptyString) == 24, "a libc++ std::string is 24 bytes");
  EmptyString noSource{};
  using SetFn = void(__fastcall*)(const void*, int32_t, EmptyString*);
  Callable<SetFn>(edge155::rva::DistilledPagePrefs_SetTheme)(prefs, theme,
                                                             &noSource);
}

inline void* ThemeServiceForProfile(void* profile) {
  using Fn = void*(__fastcall*)(void*);
  return Callable<Fn>(edge155::rva::ThemeServiceFactory_GetForProfile)(profile);
}

inline void* ThemeServiceForWindow(void* hwnd) {
  void* profile = ProfileForTabStrip(TsmForWindow(hwnd));
  return profile ? ThemeServiceForProfile(profile) : nullptr;
}

// A libc++ std::string holding a theme id, to pass by const-ref. One reused
// DLL-lifetime buffer: we clear it in place (keeps any heap buffer) and append
// the id, so we never pair an operator delete ourselves and the one small
// buffer is reclaimed when Edge exits. Not re-entrant, which the UI-thread-only
// theme callers satisfy.
inline constexpr char kNul = 0;
inline const void* ThemeIdString(const char* themeId) {
  static uint64_t storage[3] = {0, 0, 0};  // an empty libc++ std::string
  // Emptied in place (libc++ layout: a heap string has its capacity's top bit
  // set, and clearing keeps the buffer). Edge 158's assign(n, c) is not the
  // function the 155 placement found: it left a NUL in front of the id.
  const bool isHeapString = (storage[2] >> 63) != 0;
  if (isHeapString) {
    storage[1] = 0;
    *reinterpret_cast<char*>(storage[0]) = kNul;
  } else {
    storage[0] = storage[1] = storage[2] = 0;
  }
  using AppendFn = void(__fastcall*)(void*, const char*);
  Callable<AppendFn>(edge155::rva::BasicStringChar_append)(&storage, themeId);
  return &storage;
}

// Where an installed extension sits in the profile's registry.
enum class ExtensionState : uint8_t {
  kAbsent,
  kEnabled,   // loaded and on (the active theme is)
  kDisabled,  // installed but off (an inactive theme, or one not yet accepted)
};

// The installed extension with id `themeId` in `profile` and which set holds it
// (ExtensionSet::GetByID on the registry's disabled, then enabled, set).
inline void* ExtensionById(void* profile, const char* themeId,
                           ExtensionState* state) {
  *state = ExtensionState::kAbsent;
  using RegistryFn = void*(__fastcall*)(void*);
  void* registry =
      Callable<RegistryFn>(edge155::rva::ExtensionRegistry_Get)(profile);
  if (!registry) {
    return nullptr;
  }
  using GetByIdFn = void*(__fastcall*)(void*, const void*);
  const auto getById = Callable<GetByIdFn>(edge155::rva::ExtensionSet_GetByID);
  void* extension = getById(reinterpret_cast<uint8_t*>(registry) +
                                edge155::off::REGISTRY_disabled_set,
                            ThemeIdString(themeId));
  if (extension) {
    *state = ExtensionState::kDisabled;
    return extension;
  }
  extension = getById(
      reinterpret_cast<uint8_t*>(registry) + edge155::off::REGISTRY_enabled_set,
      ThemeIdString(themeId));
  if (extension) {
    *state = ExtensionState::kEnabled;
  }
  return extension;
}

// Turn on the installed-but-disabled extension `themeId` (the call Edge's own
// Enable button makes). It clears whatever disabled it, an external extension
// the user never accepted or a theme that was switched away from. Only call for
// an extension ExtensionById reported kDisabled: EnableExtension CHECKs that it
// is in the disabled set.
inline void EnableDisabledExtension(void* profile, const char* themeId) {
  using GetRegistrarFn = void*(__fastcall*)(void*);
  const void* registrar =
      Callable<GetRegistrarFn>(edge155::rva::ExtensionRegistrar_Get)(profile);
  if (!registrar) {
    return;
  }
  using EnableFn = void(__fastcall*)(const void*, const void*);
  Callable<EnableFn>(edge155::rva::ExtensionRegistrar_EnableExtension)(
      registrar, ThemeIdString(themeId));
}

// Ask Edge to install the external extension `themeId` again although the user
// once removed it: forget the "externally uninstalled" record, then have the
// external-extension loader revisit its registered extensions. The install
// finishes later, on Edge's own threads.
inline void ReinstallExternalExtension(void* profile, const char* themeId) {
  using GetFn = void*(__fastcall*)(void*);
  const void* const prefs =
      Callable<GetFn>(edge155::rva::ExtensionPrefs_Get)(profile);
  if (prefs) {
    using ClearFn = void(__fastcall*)(const void*, const void*);
    Callable<ClearFn>(
        edge155::rva::ExtensionPrefs_ClearExternalExtensionUninstalled)(
        prefs, ThemeIdString(themeId));
  }
  const void* const manager =
      Callable<GetFn>(edge155::rva::ExternalProviderManager_Get)(profile);
  if (manager) {
    using CheckFn = void(__fastcall*)(const void*);
    Callable<CheckFn>(
        edge155::rva::ExternalProviderManager_CheckForExternalUpdates)(manager);
  }
}

// Make `extension` the profile's active theme, live: DoSetTheme builds the theme
// pack and (suppress=false) notifies, repainting the browser in place. Unlike
// UseDefaultTheme, DoSetTheme takes the primary `this` (its large member offsets
// prove it; the +0x10 secondary base faults here).
inline void ThemeSetFromExtension(void* themeService, void* extension) {
  using Fn = void(__fastcall*)(void*, void*, bool);
  Callable<Fn>(edge155::rva::ThemeService_DoSetTheme)(themeService, extension,
                                                      false);
}

// Drop any extension theme and return the profile to the default theme, live.
// UseDefaultTheme is an override in a base at ThemeService + 0x10 (it adjusts
// this - 0x10 back to the primary), so it takes that secondary-base pointer.
inline void ThemeUseDefault(void* themeService) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::ThemeService_UseDefaultTheme)(
      reinterpret_cast<uint8_t*>(themeService) +
      edge155::off::THEMESERVICE_use_default_this);
}

// A color the profile's active theme extension sets, read from its theme pack.
// A default theme has no pack, and a theme leaves most colors unset.
inline theme_tone::OptionalColor ThemePaletteColor(void* themeService,
                                                   int colorId) {
  const void* pack =
      Field<void*>(themeService, edge155::off::THEMESERVICE_supplier);
  if (!pack) {
    return {};
  }
  using Fn = bool(__fastcall*)(const void*, int, uint32_t*);
  uint32_t color = 0;
  const bool isSet = Callable<Fn>(edge155::rva::BrowserThemePack_GetColor)(
      pack, colorId, &color);
  return {isSet, color};
}

// Whether the active theme of `themeService` is light or dark, read off the
// colours it paints the toolbar and frame with (theme_tone::ToneOf).
inline theme_tone::Tone ThemeTone(void* themeService) {
  return theme_tone::ToneOf(
      ThemePaletteColor(themeService, edge155::off::THEMECOLOR_toolbar),
      ThemePaletteColor(themeService, edge155::off::THEMECOLOR_frame));
}

// Tell `webContents` its color provider changed: a WebUI page fetches its
// colors.css again and the page's web preferences are computed again.
inline void NotifyColorProviderChanged(void* webContents) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::WebContentsImpl_OnColorProviderChanged)(
      reinterpret_cast<uint8_t*>(webContents) +
      edge155::off::WEBCONTENTS_color_observer);
}

}  // namespace edgeplus::edge
