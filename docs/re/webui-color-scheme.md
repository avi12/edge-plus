# Why edge://settings and edge://extensions ignore the theme's light or dark

Pinned to msedge.dll 155.0.4283.62. RVAs live in `src/offsets.h`; the calls in
`src/edge.h`; the detours in `src/theme_follow.h`; the page test in
`src/followed_pages.h`. PDB names below are from the public PDB.

## What decides a page's light or dark

Two separate things, and Edge feeds neither from the theme:

1. **`prefers-color-scheme`**, computed per WebContents. The web preferences are
   filled in `ChromeContentBrowserClient::OverrideWebPreferences` (and its
   after-navigation and need-update siblings), which call
   `EdgeComputePreferredColorScheme(out, siteInfo, webContents)`. It writes two
   dwords that become `WebPreferences + 0x380 / 0x384` (swapped by a `pshufd`):
   `out+0` is blink's `PreferredColorScheme` (**0 = dark, 1 = light**), `out+4`
   the root scrollbar scheme (1 = dark). For an ordinary page it derives them
   from the contents' color provider (`IsDark` of one color) or the OS.
2. **The colors a WebUI page draws with**: Edge injects design-system tokens and
   `chrome://theme/colors.css` from the contents' color provider
   (`WebContentsImpl::GetColorProvider`, asked by `ThemeSource::SendColorsCss`).
   That provider belongs to the browser window and its key's mode comes from
   `ThemeService::GetColorProviderKey`: the OS's mode, replaced only when the
   Appearance setting (`kBrowserColorScheme`) says Light or Dark. The theme
   extension's colors never reach it.

So a theme can paint the frame dark while the pages stay light.

## What was run (throwaway Edge, Windows in light mode)

`matchMedia('(prefers-color-scheme: dark)').matches` on edge://settings, switch
off:

| Edge "Default theme" | theme extension | frame | settings page | follows the frame |
|---|---|---|---|---|
| System | none | light | light | yes |
| Dark | none | dark | dark | yes |
| System | dark theme | dark | **light** | **no** |
| Dark | light theme | light | **dark** | **no** |
| System | light theme | light | light | yes (by luck) |

The failing cases are "a theme extension whose tone differs from the Windows or
Appearance mode". Theming's live swap hits the same hole. (Windows dark mode with
Appearance on System behaves like the Dark row; not re-run, since flipping
Windows' mode would touch the real session.)

## The lever: per WebContents, only two pages

A page is "followed" when the contents' visible URL (`WebContents` vtable +0x68,
the pending URL during a navigation, which the last-committed URL is not) is
`edge://settings` or `edge://extensions` (`followed_pages.h`). For those and only
those, while the switch is on and the profile's theme is an extension or colored
theme that sets a toolbar or frame color (`theme_tone.h`: toolbar first, luma
below 128 is dark, as Chromium's `IsDark`; read through
`BrowserThemePack::GetColor` on the supplier at `ThemeService + 0x768`, ids
toolbar 23 and frame 28):

- **Detour `EdgeComputePreferredColorScheme`**: after the original, overwrite the
  pair with the theme's tone. Every other contents keeps Edge's result.
- **Detour `WebContentsImpl::GetColorProvider`**: for a followed page, ask the
  original with a flag set that makes a detour on
  `ThemeService::GetColorProviderKey` replace the key's first field (the color
  mode, 0 light / 1 dark) for that one synchronous call. The window's own key is
  untouched, so the browser's frame and every other page keep their provider.

A profile on the default theme has no theme pack, so nothing is overridden and
Edge's Appearance setting still rules.

Taking up a change on pages that are already open:

- The switch turning on or off, per window: for each followed tab,
  `WebContentsImpl::OnColorProviderChanged` (the contents' prefs recompute) and a
  normal reload. Edge's own Appearance setting changes an open extensions page's
  media query but not the colors it loaded with (tried: `SetBrowserColorScheme`
  from the DLL on an open page, frame turned dark, page stayed white), so without
  the reload a page would be half dark.
- A theme swap by Theming: `EdgeComputePreferredColorScheme` sees a followed page's
  tone change and posts `kRedrawFollowedPages`, which does the same, at most once
  in three seconds so a page that cannot take the tone is not reloaded forever.

## Dead ends

- Overriding the window's color provider key (and `BrowserUsesDarkColors`)
  globally. It made the pages dark, but a web page's `prefers-color-scheme` comes
  from the same provider, so **every site went dark** too. Reverted.
- Reading the supplier at +0x760 of the primary `this`: that offset is the field
  as seen through the base at +8 (the real one is +0x768); the raw_ptr check in
  `GetColorProviderKey`'s callee int3'd on the wrong object.
- Building a provider for the other mode by hand (the contents' source's
  `GetColorProviderKey` into a zeroed key, then `ColorProviderManager`): crashed
  in `ColorProviderKey::operator=`; the forced-mode flag on the existing detour
  replaces it.
- Writing `kBrowserColorScheme` through `SetBrowserColorScheme`: native, but it
  rewrites the user's saved Appearance setting, affects the whole browser, and
  outlives a crash.
- Setting only `out+4` of the preferred pair: no effect on `matchMedia`; `out+0`
  is the one.

## Verified live

Throwaway Edge (own `--user-data-dir` and `LOCALAPPDATA`, DLL injected, the
config driven by window message so Avi's DLL events are never signalled),
`matchMedia` and page backgrounds read over CDP, windows captured with
PrintWindow:

| case (Windows light) | settings / extensions | https://example.com | edge://history |
|---|---|---|---|
| dark theme, System, switch on | dark, dark colors | light (unchanged) | light (unchanged) |
| light theme, Appearance Dark, switch on | light | **dark** (Edge's own Dark) | dark (Edge's) |
| default theme, Appearance Dark, switch on | dark (Edge's) | dark (Edge's) | |
| dark theme, switch toggled off/on with pages open | flips each time, colors too | unchanged | |
| Theming swap dark theme to light theme and back, pages open | flips each time, colors too | unchanged | |

Not exercised: Windows' own dark mode (would flip Avi's session), Edge's
chrome-untrusted pages (left alone by construction).
