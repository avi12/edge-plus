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

## The Appearance remix's `darkmode` [run]

An Appearance remix is stored as plain JS plus its arguments (`default_arguments`
row, function `applyColorWheelThemeV2`, `args` = JSON text per argument,
`"\"true\""` for the third, `darkmode`). Edge runs it as two script shapes:

- An editor apply (and the stored remix Edge applies when the editor is opened
  through the remix's **Edit**) is built by `RemixAction_BuildScript` (hot stub
  0x100e633f into the body at 0x820998e; one caller, the executor at 0xb7d7074):
  `(function() { try { NAME(arg,...); } catch(e) {...} })();`.
- A stored remix applied as its page loads never passes that builder: it arrives
  as UTF-16 `(function(){try{NAME(arg,...);return true;}catch(e){return false;}})();`,
  after the template itself (`function applyColorWheelThemeV2(...) {...}`, 27 KB).

Both reach the page through `RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld`
(0xf437d30) in world 4, so that is the one place they are rewritten. The same world
carries Read Aloud's bundles (`learningToolsRuntime`, webpack, tens of kilobytes), so
the detour reads only world-4 scripts of at most 4096 characters while the remix
switch is on, passes every other call straight on, and counts itself
(`edge::RunningDetour`) so unload waits for no thread to be inside it. Built on
34e4ed64, which hooked only the builder, every load-time apply ran Edge's stored
darkmode and colour: pages loaded in Edge light came up in the stored dark remix
(seen: all four test pages `hue=228 dm=true` with Edge light).

**The template's `darkmode` inverts, it does not darken** (template read from a
kept bundle row, 2026-11-23): with `"true"` every page colour's lightness is
flipped (`l -> 1 - l`), so a page already dark from Edge's dark mode
(`prefers-color-scheme`, e.g. google.com) comes out light, while a light page
comes out dark. So the call becomes a request to a helper of the page's own
(`remix_arguments::WithPageAwareTheme`, `kHelperScript`, installed in world 4
before it): darkmode is `target-is-dark XOR page-is-dark`, the page's darkness
first guessed by sampling rendered backgrounds at a few viewport points (the
template's leftovers switched off), then checked after the asynchronous apply
settles: a miss is read again 600 ms later, then re-applied once with the
opposite darkmode; when neither reaches the target the nearer of the two is
kept and root and body are tinted (a see-through page under the other
prefers-color-scheme is painted by prefers-color-scheme whatever darkmode says;
a dark page's coloured backgrounds are lifted to mid tones either way). The
outcome is left as `<html data-edgeplus-remix="target=..;pageDark=..;flipped=..;
reapplied=..;forced=..;matched=..;applies=..;skips=..">`.

**Every apply is serialized through that helper's `request`.** The template's
`me()` sets `__themeState.lastApplyKey` before its asynchronous work, restores
the page's originals and aborts the apply in flight on a new key; it returns at
once on the key it last saw. So two appliers (Edge's call, the DLL's push, the
page's own listener) could abort each other half way, and the old check judged a
page from a sample taken mid-apply (it gave up waiting after 8 s, which a hidden
tab's throttled timers always reach): it then flipped a correct apply to the
wrong darkmode and tinted root and body over light content, the remix looking
cleared (34e4ed64, seen on the dark-aware test page: light cards on a dark
tint). Now one request runs at a time and the latest wins; the apply in flight
is let finish first (1.5 s at most while shown); the key is forgotten whenever
the page is not themed or an apply never finished; a hidden tab's check waits
for the page to be shown rather than give up; and a request for exactly what
the page already wears (same mode, colour, font, zoom and prefers-color-scheme,
themed, nothing in flight) makes no apply. The target is the open editor's previewed mode (its Light | Dark
selector, published as `;preview=` in the editor's address fragment and read
at script build time with the pair it holds), else Edge's mode. Verified in a
throwaway Edge: google dark + `"true"` -> body rgb(212,214,222); through the
helper -> rgb(33,35,43).

**Edge's mode flipping under an applied remix.** Edge re-runs a remix only on
the next load. Create a remix runs its scripts in isolated world 4 (the executor
at 0x73f5952 calls the main frame's `ExecuteJavaScriptInIsolatedWorld`, vtable
slot 0x1d0, with 4; `kRemixWorldId`). A tab's `prefers-color-scheme` change
event does fire when Edge's Appearance flips, but a background tab gets it only
once it is shown. So the remix poll watches Edge's mode (`g_edgeTone`) and on a
flip runs `FollowModeScript` in world 4 of every open tab: a page wearing an
Appearance remix (`window.__blueprintParams`, any build or Edge alone) gets the
helper and a request in the new mode's saved colour. The push is the one
authority on the target (the rewritten calls use the same `g_edgeTone`); the
helper's own `matchMedia` listener only asks for the same request again when the
page's prefers-color-scheme flips, since that changes the page's own colours the
darkmode was chosen against (on 34e4ed64 it followed the page's scheme and stood
aside for two seconds after a push, so a scheme flip landing in that window left
the page judged under the old scheme). Enabling a theme extension (the Edge+ pair's swap, or
installing one) neither resets world 4 nor makes Edge run any remix script: in
the throwaway the page helpers kept their state across every swap and Edge's
world-4 theme calls stayed at the count the page loads made.

**Opening the editor through Edit applies the stored remix.** Within a second of
the remix's More options > Edit, Edge runs the stored arguments once through the
builder (`applyColorWheelThemeV2("228","20.069..","true",...)`, the editor's
preview not yet published, so the target is Edge's mode). On 34e4ed64 that
re-themed the page each time (Avi's two applies of 166,27 on opening). The
rewrite gives it the saved colour of Edge's mode, which is what the page already
wears, so the helper counts it a skip and makes no apply. Create > Appearance
made no call at all; the wheel bridge (`setKnobFromHueIntensity`) moved the
knob with no apply either way.

`remix_darkmode.h` detours `ExecuteJavaScriptInIsolatedWorld`, and with the remix
switch on rewrites a `__edgeRemixApplyTheme` / `applyColorWheelThemeV2` call into the
helper's request (`remix_arguments.h`, pure, `test/theme/remix_arguments_test.cpp`):
its darkmode argument (`"true"`, `true` or `1`, their opposites, or empty) is left
to the helper and its colour set to the saved one for the target mode; any other
shape, or another remix's call, is left alone. The rewritten script goes to the
original in a string of its own, freed after. "Edge's own mode" is the signal
the followed pages use: the theme's tone, else Edge's own preferred scheme (which
includes the Appearance setting), read in `theme_follow.h`'s
`EdgeComputePreferredColorScheme` detour for every page that is not itself a followed
page (`remix_darkmode::ObservePage`), so it is that of the page the script is about to
run in. Until a page has been computed since the DLL loaded the mode is unknown and no
argument is touched; Windows' setting is never consulted.

Placement: `ExecuteJavaScriptInIsolatedWorld` is a core entry (every script the DLL
runs goes through it), so a build that cannot place it hooks nothing at all.

Checked live (throwaway Edge 158, this build injected, a stored "Always" remix seeded
for two local test pages, google.com and a Wikipedia article, all four in background
tabs, saved Light 270,60 and Dark 120,50): Appearance flips four times, rapid
dark-light-dark and light-dark-light flips, the Edge+ dark theme enabled and a light
theme installed in turn (also rapid), and Appearance and theme flipped together: 0
pages wrong or cleared in 84 page checks; the dark-aware page under a dark theme with
Edge's Appearance left light reaches only a mid-tone green (the template's limit, kept
as the nearer darkmode and tinted).

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

**The editor's wheel shows the selected mode's colour without applying.** The wheel is the `widget-adjustment` component (FAST) on edge://blueprint; its `setKnobFromHueIntensity(hue, intensity 0..1)` only moves the knob (`knobX`/`knobY`/`knobTouched`), while an apply happens only on a pointer release or a key (`applyThemeCustomizer`). The component lives in the page's own world, so the DLL runs a small bridge there through `RenderFrameHostImpl::ExecuteJavaScript` (0xf437ce0, the vtable slot before the isolated variant; it CHECKs `CanExecuteJavaScript`, 0xf437ca6, which is asked first and holds for a WebUI page), and the editor script asks it through an attribute and an event on the shared DOM, on opening, reopening and an Edge flip under an untouched selector. Checked in a throwaway Edge on the real editor: 0 applies on opening, reopening and flips; one per segment click.
