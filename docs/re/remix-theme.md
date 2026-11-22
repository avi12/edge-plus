# Create a remix follows Edge's light or dark (Edge 158.0.4370.33)

Edge's "Create a remix" (internal name Blueprint) is a WebUI. The DLL's
`remix_mode.h` switch makes its two pages follow Edge's own light or dark and the
Edge+ theme, the way `theme_follow.h` does for settings and extensions
(`webui-color-scheme.md`). Default off.

Evidence: **[run]** in a throwaway Edge 158 (own `--user-data-dir` marker, its own
`LOCALAPPDATA`, the DLL as a timestamped copy from `app/dll/out/`, inspected over
its own `--remote-debugging-port`, captured with `PrintWindow`); **[static]**
msedge.dll 158 disassembly; **[unit]** `test/theme/followed_pages_test.cpp`.

## What ships

- `followed_pages.h` knows `edge://blueprint` (the Create a remix window, with its
  Appearance and Custom choices and the editor) and `edge://remix` (the manager
  side panel). `Follows(url, Switches)`: each page group follows only while its own
  switch is on, so the settings switch no longer implies the remix pages and the
  remix switch does not touch settings.
- `theme_follow.h`'s three detours (`EdgeComputePreferredColorScheme`,
  `WebContentsImpl_GetColorProvider`, `ThemeService_GetColorProviderKey`) are
  shared; no new RVA. They are installed when either feature is available. The
  entries carry both feature bits (`EdgePlus.Signatures.psm1`), so the remix switch
  stands down alone when one is unplaced.
- Wire: `remixThemeActive`, appended after the hold id (wire 244 bytes), so a config
  file written before it still loads, as off. Panel: `Feature::RemixTheme`, serde
  `remixTheme`, a card, wording in `shared-wording.json`.

"Edge's own mode" here is the same signal as the settings pages: under a theme
extension the tone of its toolbar or frame colours (`theme_tone.h`); with none,
Edge's own result, which already includes the Appearance setting and is not
Windows'.

## Checked live [run]

Windows dark, Edge's Appearance setting Light, a dark theme extension, the page
opened as a tab at `edge://blueprint`:

| switch | `prefers-color-scheme: dark` | page background | text |
|---|---|---|---|
| off | false | `rgb(246,246,246)` | `rgb(16,16,16)` |
| on | true | `rgb(51,51,51)` | white |

So with the switch on the page takes the theme's tone, not Windows'. With no theme
(Edge Light, Windows dark) the page was light with the switch off, which is Edge's
own mode.

The page's own colours come from `--smtc-*` / `--mai-smtc-*` custom properties
(205 on the page), which Edge derives from the same provider, so no style injection
is needed for light and dark.

## The Appearance remix's `darkmode` [static, built; not seen firing live]

An Appearance remix is stored as plain JS plus its arguments (`default_arguments`
row, function `applyColorWheelThemeV2`, `args` = JSON text per argument,
`"\"true\""` for the third, `darkmode`); the load-time wrapper `__edgeRemixApplyTheme`
(callee 0x104f260, once per page load) only names the parameter, so there is nothing to
rewrite there. What does run per apply is the script builder:
`RemixAction_BuildScript` (`offsets.h`, hot stub 0x100e633f into the body at
0x820998e; one caller, the executor at 0xb7d7074, which hands the result to the
page). It joins the action's function name and its arguments (each parsed from its JSON
text and written back as a JS literal, `0x100e6288`) into
`(function() { try { NAME(arg,arg,...); } catch(e) {...} })();`, UTF-8, then
`UTF8ToUTF16` into its `std::u16string` result.

`remix_darkmode.h` detours it, and with the remix switch on rewrites the third argument
of a `__edgeRemixApplyTheme` / `applyColorWheelThemeV2` call to Edge's light or dark
(`remix_arguments.h`, pure, `test/theme/remix_arguments_test.cpp`): `"true"`, `true`
or `1` and their opposites, in the form already there; any other shape, or another
remix's call, is left alone. The result string is replaced whole (freed with Edge's
own `operator delete`, rebuilt with `UTF8ToUTF16`). "Edge's own mode" is the signal
the followed pages use: the theme's tone, else Edge's own preferred scheme (which
includes the Appearance setting), read in `theme_follow.h`'s
`EdgeComputePreferredColorScheme` detour for every page that is not itself a followed
page (`remix_darkmode::ObservePage`), so it is that of the page the script is about to
run in. Until a page has been computed since the DLL loaded the mode is unknown and no
argument is touched; Windows' setting is never consulted.

Placement: the entry is in the signature table under the RemixTheme feature alone, so
a build that cannot place it stands the remix switch down (the followed pages with it)
and nothing is hooked.

Not seen live: in a throwaway Edge 158 (fresh profile, Windows account signed in by
single sign-on, the remix prefs and `edge-remix` labs flags set, a seeded Appearance
remix whose row Edge's own manager `edge://remix` lists as "Always") no automatic
remix ran on navigation and the builder was never called, so the argument was not
seen flipping. The profile lacks whatever field trial turns Edge's trigger on; the
on-demand route is behind the toolbar button's native menu. Try it in a profile where
a remix already works.

## Not verified

- The real bubble (`BubbleView`, a views bubble hosting the same WebUI): only the
  `edge://blueprint` tab was inspected. Whether the bubble widget's colour provider
  goes through `ThemeService::GetColorProviderKey` (risk 1) and what its
  `VisibleUrl` is during load (risk 2) are still open. An open bubble is not a tab,
  so `RefreshPages` does not redraw it on a flip; reopening it does.
- `edge://remix` (not opened).
- Custom remixes: out of scope.

## The Cyberpunk skin on these pages [run]

`remix_skin.h` (pure: the style text from `cyberpunk_palette.h` roles, the paint and
remove scripts) and `remix_skin_mode.h` (a 400 ms window timer, as reading mode's)
inject one `<style class="edgeplus-remix-skin">` into the document and every open
shadow root of each `edge://blueprint` / `edge://remix` page, through the isolated
world (`RunScriptUnanswered`). Gate, `remix_skin::ShouldSkin`: remix switch AND
`tab_shape::g_isCyberpunkSkinOn` (the flag every skin hook reads). Skin off: no
style, stock look (captured, `out/shots-re/gate-skin0-1.png` vs `gate-skin1-1.png`).
The light or dark follow does not depend on the skin.

What the page is: `widget-adjustment` (the Appearance editor: `.color-picker` wheel,
`.lights-button`, `.zoom-controls`, `.type-selector` of `mai-button.type-button`) and
`blueprint-main` (`.card` choices), all Fluent `mai-button`s styled by `:host` rules
and `--smtc-*` tokens; the "Dark" button is `appearance="Outline"` (capital O, so
none of the component's `[appearance="outline"]` rules match) with `aria-pressed`.
The skin repoints the tokens (`kPageTokens`, incl. the editor's `--blueprint-dark-*`)
and restyles shapes with `!important`: no radius, 7px / 12px chamfers, 2px teal idle
lines, mono labels, red wash plus red outline for the pressed/checked, 2px title rule
with the 44px red stub. The hue wheel stays round and functional.

Not reached: the real bubble (not a tab; needs the Remix cloud), the light tone live
(a forced tone did not reach the poll in the throwaway), `edge://remix`.
Placed for 158 to run scripts: `WebContentsImpl_GetPrimaryMainFrame` 0x1135fb0 (field
+0x968, next to GetController) and `RenderFrameHostImpl_ExecuteJavaScriptInIsolatedWorld`
0xf437d30 (hot stub into 0x5bc5e9a). A script with no answer builds no callback, so
it needs no `BindStateBase_ctor` (still unplaced; reading mode stays down).
