# Reading mode's page theme (Edge 155.0.4283.62)

Edge's reading mode (the `read://` page, Edge's Immersive Reader built on
Chromium's DOM distiller, not Chromium's Read Anything side panel) has an
Appearance settings flyout whose **Themes** grid offers twelve page colours. The
DLL's `reading_mode.h` sets one of them for Windows light and another for dark.

Evidence: **[static]** from the public PDB and the binaries; **[run]** in a
throwaway Edge (own `--user-data-dir` marker `edgeplus-readtheme-3b9d`, its
`LOCALAPPDATA` redirected so its DLL read a private config, a Wikipedia article
opened as `read://https_en.wikipedia.org/?url=...`, inspected over its own
`--remote-debugging-port`); **[unit]** `test/theme/reading_themes_test.cpp`.

## Where the theme lives

- **The pref.** `reading_view.theme` (an int) in the profile's prefs, beside
  `reading_view.font_size`, `.font_type`, `.column_type`, `.text_spacing`.
  [static: the strings in msedge.dll; run: a profile's Preferences]
- **The values.** `learning_tools.dll` carries the theme table the reading view
  uses (`edge::learning_tools::GetReadingViewThemes` loads it from that DLL): 23
  entries keyed `"0"` to `"22"`, each a `themeName` and a background. The grid
  shows twelve of them; `reading_themes.h` names them, in grid order:

  | grid | value | themeName | background | page class |
  |---|---|---|---|---|
  | row 1 | 0 | Sepia | `#F9F5E9` | `ms-theme-sepia` |
  | | 1 | Light | `#FCFCFC` | `ms-theme-light` |
  | | 2 | Grey | `#E6E6E6` | `ms-theme-grey` |
  | | 5 | Blue | `#87faff` | `ms-theme-blue` |
  | | 6 | Yellow | `#feff5c` | `ms-theme-yellow` |
  | | 4 | Green | `#91ffa6` | `ms-theme-green` |
  | row 2 | 18 | Lavender | `#d1bfeb` | `ms-theme-lavender` |
  | | 17 | LightBlue | `#b3caec` | `ms-theme-lightblue` |
  | | 16 | SkyBlue | `#a3cfe4` | `ms-theme-skyblue` |
  | | 12 | LightGreen | `#a5da90` | `ms-theme-lightgreen` |
  | | 3 | Black | `#000000` | `ms-theme-black` |
  | | 22 | DarkGrey | `#242424` | `ms-theme-darkgrey` |

  [static: the table; run: each value set in the page gave the class shown]

## How it is applied

- **The page** runs `readingViewController.setPreferences({theme: n})` (its main
  world). That switches it live but does **not** persist: the pref did not
  change. [run] The page's CSP is `script-src 'self'`, so the DLL (whose scripts
  run in an isolated world) cannot reach it that way.
- **The browser side** owns the pref: `dom_distiller::DistilledPagePrefs`
  (`SetTheme` `0xbf84f80`). `SetTheme(this, Theme, std::string source)` writes
  `reading_view.theme` through `PrefService::SetInteger`, then posts
  `NotifyOnChangeTheme`, whose observers
  (`DomDistillerRequestViewBase::OnChangeTheme`) run
  `dom_distiller::viewer::GetReadingViewSetThemeJs` — the same
  `readingViewController.setPreferences(` call — in every open reading view.
  `source` (by value; the callee frees it) names a viewer to skip; empty skips
  none. [static]
- **Reaching it.** `DomDistillerServiceFactory::GetForBrowserContext`
  (`0x2e52da0`) gives the profile's `DomDistillerContextKeyedService`; its
  `DomDistillerService` base is at `+8`, and that service's `DistilledPagePrefs*`
  at `+0x20` — exactly what `ChromeDomDistillerClient::GetDistilledPagePrefs`
  does. [static]

So `edge::SetReadingTheme(browserContext, n)` is one call that persists the theme
for every later reading view and switches the open ones. [run: with the switch on
and Lavender for light, injecting the DLL turned the open view `ms-theme-lavender`
and wrote `"theme": 18` to the profile; changing the light choice to Black then
turned it `ms-theme-black` live]

## The Appearance flyout (where the two choices are made)

Reading mode's toolbar is not browser UI: it is the page's own
`<reader-mode-bar>` element, whose **open** shadow root holds a Fluent web
component toolbar (`fluent-button#settings-button` opens the settings menu,
`fluent-menu-item.appearance-menu-item` its Appearance settings). The flyout is
an `<appearance-pane>` with its own open shadow root: Font, Font size, Column
width, then `.themes-container` ("Themes" in a `.title-font`, then
`.color-theme-section`). The grid is a `fluent-radio-group` (aria-label "Color
theme", `value` = the current theme number) of twelve `fluent-field`s, each a
`label.color-label` (`.color-button-container` > `.color-button`, plus a
`.checked-indicator` with a check SVG on the selected one) and a
`fluent-radio` whose `value` is Edge's theme number and whose `title` is the
swatch's name ("Sepia", "Light", "Grey", "Blue", "Yellow", "Green", "Lavender",
"Light blue", "Sky blue", "Light green", "Black", "Dark grey"). The flyout is
styled with Fluent tokens (`--smtc-*` custom properties: foreground, background,
stroke, corner, focus stroke, caption font sizes) and Segoe UI Variable. [run]

**Adding UI there.** The page's CSP (`script-src 'self'`) keeps out any script
of the page's own kind, but the DLL's page scripts run in its isolated world
(`RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld`, world 11, as the find
and chat scripts do), which that CSP does not govern; the DOM, the open shadow
roots included, is shared, so the isolated world can insert elements into the
flyout and listen to its events, and `style-src 'unsafe-inline'` lets it add a
`<style>` to the pane's shadow root. The page's own globals
(`readingViewController`) stay out of reach, which is why the theme is still set
through `DistilledPagePrefs::SetTheme`. [run: the switch below was added by the
DLL; the page's main world never saw its state object]

`reading_scripts.h` (run by `reading_mode.h` every 400 ms in every live
`read://` page while the switch is on):

- When the flyout is shown it adds, under "Themes", a `role="radiogroup"`
  ("Themes for") of two `role="radio"` buttons, **Light mode** and **Dark mode**,
  in the flyout's own tokens (caption size, subtle-selected ground, neutral
  stroke, the Fluent focus outline), keyboard-operable (Tab, arrows). It opens on
  the side Windows is in.
- On Windows' own side the grid is the page's: a pick switches and saves the page
  theme as before and is also noted. On the other side a pick is caught in the
  capture phase (click and keyboard) before it reaches the radios, so the page
  keeps its theme; the page's checkmark is hidden and a clone of it marks that
  side's choice.
- Each visit answers `ok`, or `ok US light US dark` after a pick. The DLL keeps
  the pair in `%LOCALAPPDATA%\EdgePlus\reading-themes.bin` (two little-endian
  int32s, light first; `reading_themes::ChoicesFromBytes`), and sets the theme at
  once when the current mode's choice changed.
- Switched off (or on unload), `RemoveScript` takes the switch, its style and its
  marks out, and the grid's listeners let every pick through, so the flyout is
  exactly Edge's own.

[run, throwaway: Lavender picked on the light side switched the page and saved
18; Dark grey picked on the dark side left the page Lavender and saved 22; after
closing and reopening the flyout the switch opened on Light mode with Lavender
checked and showed Dark grey on Dark mode; with the page set to Yellow by hand,
the DLL's apply (`kApplyTheme`) returned it to Lavender; switched off, the switch,
its style and its marks were gone]

## When

With the panel's switch on (`config.h`'s `readingThemeActive`; the two themes
are no longer on the wire), the theme for the current Windows mode is set when
the switch comes on, on every light/dark flip, on the signal theming already uses
(`WM_SETTINGCHANGE "ImmersiveColorSet"` -> `kApplyTheme` -> `control.h`'s
`ApplyForCurrentOsMode`), in the profile behind each browser window, and when the
person picks a new theme for the current mode in the flyout. [run: the switch,
the pick and `kApplyTheme`; the flip itself is the existing path, not run since
the OS mode was not flipped]
