# Why edge://settings and edge://extensions ignore the theme's light or dark

Pinned to msedge.dll 155.0.4283.62. RVAs live in `src/offsets.h`; the calls in
`src/edge.h`; the detours in `src/theme_follow.h`. PDB names below are from the
public PDB.

## What the pages follow today

A page's `prefers-color-scheme` (and so the WebUI's own dark mode, which is a
`@media (prefers-color-scheme: dark)` block) comes from the **color mode of the
browser's color provider key**, not from the theme's colors:

```
ThemeService::GetColorProviderKey(this, out, nativeKey, profile)
  out = copy of nativeKey            // mode = Windows' light/dark
  if GetBrowserColorScheme() != 0    // Edge's "Default theme" setting
      out.color_mode = that setting  // (kBrowserColorScheme, 0 system / 1 light / 2 dark)
WebContents color mode  ->  web preferences  ->  prefers-color-scheme
```

`ThemeService::BrowserUsesDarkColors` (the question Edge's own views ask: icons,
find bar, new tab page) takes the same two inputs. Neither reads the active theme
extension. So a theme can paint the frame dark while the pages stay light.

## What was run (throwaway Edge, Windows in light mode)

`matchMedia('(prefers-color-scheme: dark)').matches` on edge://settings, with no
DLL feature:

| Edge "Default theme" | theme extension | frame | settings page | follows the frame |
|---|---|---|---|---|
| System | none | light | light | yes |
| Dark | none | dark | dark | yes |
| System | dark theme | dark | **light** | **no** |
| Dark | light theme | light | **dark** | **no** |
| System | light theme | light | light | yes (by luck) |

So the failing cases are exactly "a theme extension whose tone differs from the
Windows / Appearance mode". The Theming feature's live swap (theme.h) hits the
same hole: swapping a dark theme in on a light Windows leaves the pages light.
(Windows dark mode with Appearance on System behaves like the Dark row; not
re-run, since flipping Windows' mode would touch the real session.)

## The lever

Detour `GetColorProviderKey` and, after the original fills the key, overwrite
`color_mode` (the key's first field) with the tone of the active theme. Detour
`BrowserUsesDarkColors` the same way so Edge's own views agree. Both only act
while the feature is on **and** the profile has a theme pack that sets the
toolbar or frame color; a default-theme profile keeps Edge's answer, so Edge's
own Appearance setting still rules there.

- The tone is `theme_tone.h`: toolbar color if the theme set it, else the frame
  color, luma < 128 is dark (Chromium's `color_utils::IsDark`). The colors come
  from `BrowserThemePack::GetColor` on the supplier at `ThemeService + 0x768`
  (`SwapThemeSupplier` writes it; `GetThemeSupplier` reads +0x760 on the base at
  +8). Ids: toolbar 23, frame 28 (found from the manifest-key table beside the
  strings "toolbar" and "frame"; confirmed live, a navy toolbar reads dark).
- Turning the feature on or off, and every theme change, re-run the question:
  `ThemeService::NotifyThemeChanged` (the call `SetBrowserColorScheme` ends in)
  makes the color providers and each open page's preferences recompute. Pages
  already open flip without a reload.
- It rides the provider mode, so it is browser-wide for the profile, exactly as
  Edge's own Appearance setting is: ordinary websites' `prefers-color-scheme`
  follows the theme too. The page preference is computed per WebContents in
  `EdgeComputePreferredColorScheme` (anonymous namespace, 0x548333b); narrowing to
  edge:// pages only would mean detouring that function, which also feeds
  Copilot/contextual pages. Not done.

## Dead ends

- `BrowserUsesDarkColors` alone: detoured first; Edge's views followed, the pages
  did not. `GetColorProviderKey` never calls it (it reads the pref inline).
- Writing `kBrowserColorScheme` through `SetBrowserColorScheme`: native and
  complete, but it rewrites the user's saved Appearance setting and would have to
  be restored (and survives a crash). A detour is reverted by unloading.
- Reading the supplier at +0x760 of the primary `this`: that offset is the
  field as seen through the base at +8 (the real one is +0x768); the raw_ptr
  check inside `GetColor` int3'd on the wrong object.

## Verified live

Throwaway Edge, own `--user-data-dir` and `LOCALAPPDATA`, window captured with
PrintWindow, `matchMedia` read over CDP: the table above flips as follows with
the feature on: dark theme on System -> dark; light theme on Dark -> light;
default theme on Dark -> unchanged (dark); light theme on System -> light;
toggling the switch off and on flips open pages both ways; Theming's live swap
light theme <-> dark theme flips open pages each time.
