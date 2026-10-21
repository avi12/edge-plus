# Reading mode's page theme (Edge 155.0.4283.62)

Edge's reading mode (the `read://` page, Edge's Immersive Reader built on
Chromium's DOM distiller, not Chromium's Read Anything side panel) has an
Appearance settings flyout whose **Themes** grid offers twelve page colours. The
DLL's `reading_mode.h` sets one of them for Windows light and another for dark.

Evidence: **[static]** from the public PDB and the binaries; **[run]** in a
throwaway Edge (own `--user-data-dir` marker `fixedge-readtheme-3b9d`, its
`LOCALAPPDATA` redirected so its DLL read a private config, a Wikipedia article
opened as `read://https_en.wikipedia.org/?url=...`, inspected over its own
`--remote-debugging-port`); **[unit]** `test/reading_themes_test.cpp`.

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

## When

With the panel's switch on (`config.h`'s `readingThemeActive`, then the light and
dark values), the theme for the current Windows mode is set when the config
arrives and on every light/dark flip, on the signal theming already uses
(`WM_SETTINGCHANGE "ImmersiveColorSet"` -> `kApplyTheme` -> `control.h`'s
`ApplyForCurrentOsMode`), in the profile behind each browser window. A theme the
person picks in the flyout stands until the next flip or config change.
[run: the config path; the flip path is the existing one, not run here since the
OS mode was not flipped]
