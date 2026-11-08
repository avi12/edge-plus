# The Cyberpunk skin: frames, title font, omnibox frame

Pinned build **156.0.4314.58** (PDB `03775A411C2857584C4C44205044422E1`). Every
RVA below is in `app/dll/src/edge/offsets.h`; this page is the evidence.

What the kit (the Cyberpunk UX kit's `README.md`, `primitives.css`,
`controls.css`) asks for, and where it stands in Edge's browser UI:

| Element | Status | Where |
|---|---|---|
| 2px frame on the selected tab (red dark, ink light) | done | `chrome/tabs/tab_frame.h` |
| 2px idle frame on other tabs (teal dark, slate light) | done | `chrome/tabs/tab_frame.h` |
| Mono tab titles (Share Tech Mono, else Consolas) | done | `chrome/tabs/tab_font.h` |
| Omnibox framed like a text field (idle line colour) | done, 1px | `chrome/toolbar/omnibox_frame.h` |
| Uppercase titles | feasible, not done | see "Next" |
| Letter-spacing on titles | not feasible | `gfx::RenderText` has no tracking |
| Near-sharp omnibox corners | feasible, not done | see "Next" |
| A colour id that outlines the selected vertical tab | none on this build | see below |

All of it rides `tab_shape::g_isCyberpunkSkinOn`; off is Edge's own look. The
colours are `cyberpunk_skin.h`'s (`FramesFor`, `FieldFrameFor`,
`TitleFontFamilyFor`), tested in `test/theme/cyberpunk_skin_test.cpp`.

## 1. Tab frames: Edge's own stroke, handed a width and a colour

No new painting. Edge already strokes the **active** tab:

`EdgeTabStyleViews::PaintTabBackground` (`0x27d8cae`) after the fill:

```
27d8d08: call views::Widget::ShouldPaintAsActive
27d8d10: xor  edx,0B13h            ; ui id 0xb12 active / 0xb13 inactive
27d8d19: call ui::ColorProvider::GetColor
27d8d1e: mov  ebp,eax              ; the stroke colour
...
27d8d8f: mov  rdx,rsi ; canvas
27d8d92: mov  r8d,ebp
27d8d95: call EdgeTabStyleViews::PaintBackgroundStroke
```

`EdgeTabStyleViews::PaintBackgroundStroke(this, canvas, SkColor)` (`0x27d8ffa`):

```
27d9036: mov rcx,[rcx+10h]          ; the tab (Tab base of the EdgeTab)
27d903d: mov rax,[rax+28h]          ; EdgeTab::IsActive (vtable 0x1177a720 +0x28)
27d9047: test al,al / je return
27d904f: mov rcx,[rax+548h]         ; the tab's controller (EdgeTabStrip's TabSlotController)
27d9059: mov rax,[rax+280h]         ; TabStrip::GetStrokeThickness
27d9066: test eax,eax / jne 0x475647c (the stroke)
```

The cold part (`0x475647c`) is Chromium's stroke: `ScopedCanvas`, `FlipIfRTL`,
`UndoDeviceScaleFactor`, a `cc::PaintFlags` with the colour (`SkRGBA4f::FromColor`),
`kStroke_Style` (`or eax,40h` in the style bits) and width `thickness * scale`,
then `GetPath(kBorder = 1)` and `gfx::Canvas::DrawPath`.

Why Edge never strokes a vertical tab: `TabStrip::GetStrokeThickness`
(`0x26edbc0`; `EdgeTabStrip` keeps it in its `TabSlotController` vtable
`0x11741b10` at `+0x280`) returns `BrowserView::ShouldDrawTabStrokes`, which
returns false whenever `BrowserView::ShouldDrawVerticalTabStrip`. Every row of
the vertical list still goes through `PaintTabBackground` and so through
`PaintBackgroundStroke` (its other branch only picks the overline colour).

So two value-only hooks frame the selected tab, the only one the kit's tab
control (`controls.css` `.c-tabs`) frames:

- `PaintBackgroundStroke`: with the skin on, for the active tab, swap the
  colour argument for the accent and set the width for the length of the call.
- `TabStrip::GetStrokeThickness`: that width while a frame is stroked, Edge's
  answer otherwise (so nothing else that asks changes).

Framing the idle tabs too (an earlier cut of the skin) took a third:
`EdgeTab::IsActive` (`0x1084810`, 15 bytes: `cmp byte [rcx+930h],0`,
`je Tab::IsActive`, `xor eax,eax; ret`), answered yes once for an idle tab, for
`PaintBackgroundStroke`'s first question only; `GetPath` asks again and hears
the truth. An idle row's border runs along its bounds, which clip the stroke's
outer half, so it needed twice the width.

The stroke goes along `GetPath`'s border, which `tab_shape.h` already squares,
so the frame follows the outline with no extra work.

The tone (dark or light) is `skin_tone.h`'s: the theme pack's, recorded when
`hooks.h` reads its colours, else Windows' light or dark (Edge's own theme has
no pack).

**Runtime finding: an idle vertical row clips half its stroke.** Measured at
5 DIP: the selected row's border path is inset (all five pixels inside, on
every side); an idle row's runs along the row's bounds (the outer half
clipped, on every side). Horizontal tabs are not clipped. A horizontal
selected tab's border is open at the bottom, as Edge draws it.

**The 0xc19 / 0xc1a ids** (`kColorEdgeVTabOutlineSelected*` in an old build):
no `mov edx, 0C19h` exists in this image, and the one `mov edx, 0C1Ah` is in
`ManagedUserProfileNoticeHandler`, so they are gone or renumbered; the earlier
"no ui id outlines the selected vertical tab" stands. The stroke above is the
route instead.

Crash risk: low. Nothing of ours paints; Edge's own stroke runs with Edge's own
flags and path. The one-shot "is active" flag is cleared after the call whatever
happened, so it cannot leak into a later question.

## 2. Tab titles in the kit's mono

`Tab::Tab` (`0x27cbaee`) builds the title as a `TabTitle` (a `views::Label`,
vtable `0x118542c0`) and stores it at `Tab + 0x950`
(`27cbd84: mov [rsi+950h],rbx`); `Tab::UpdateForegroundColors` inks the same
field. Its font is set **once**: `EdgeTab::EdgeTab` (`0x27ca680`), only when
`EdgeTabStyle::IsTokenizedTabStyleEnabled`, calls
`TypographyProvider::GetFont(0x14, 0x1000)` and the label's `SetFontList`
virtual (`[vtable+398h]`); otherwise the label keeps its default font. Nothing
sets it again (`Tab::ActiveStateChanged` here does not), and
`EdgeTabStyleViews`' own three `FontList`s (`+0x18/+0x20/+0x28`, weights
400/600/700 off `Label::GetDefaultFontList`) are not handed to the title.

So `tab_font.h` sets it itself, from `EdgeTabStyleViews::PaintTab`
(`0x27d88f0`, every tab, every row): compare the label's current font
(`Label + 0x398` is its `RenderText`, whose `FontList` is at `+0x48`, as
`Label::SetFontList` and `Label::EdgeUpdateFontList` read them) with ours by
impl pointer, and if it differs call the label's `SetFontList` virtual (slot
`0x398`, `views::Label::SetFontList` in `TabTitle`'s vtable too). Off puts back
a copy of Edge's own title font (`FontList` copy ctor `0x1016482`).

Our font: `gfx::Font::Font(const std::string&, int)` (`0x26aa68c`, a
`scoped_refptr<PlatformFont>`) then `gfx::FontList::FontList(const Font&)`
(`0x2b3b05e`), at `FontList::GetFontSize` (`0x104b740`) of Edge's font. The
family string is a libc++ short string built by hand: 23 chars then the size
byte, top bit clear, the layout `TabTitle::TabTitle` reads a `u16string` in
(`movsx rax, byte [rdi+17h]; test; jns` -> inline). Both families fit (a
`static_assert`), so no heap is involved. Share Tech Mono is used when GDI
finds it installed, else Consolas.

Crash risk: low to moderate. The swap runs during the parent tab's paint;
`SetFontList` only invalidates layout and schedules a paint, so the next frame
lays the title out in the new font. The font objects are kept for the
process's life (a few bytes, never freed), so a label never holds a dangling
font after this DLL unloads.

## 3. The omnibox frame

`LocationBarView::RefreshBackground` (`0x2840812`) picks the omnibox fill
(ui ids `0x822`/`0x823`, or `0xbc1`, `0x904`, `0xbc3`) and a stroke colour
that stays 0 unless the `kOmniboxLocationBarStrokeAndHoverRefresh` feature is
on (then `0xbc5` -> `0xbc6` with the hover), and hands both to
`LocationBarView::EdgeCreateRoundRectBackground(this, out, bg, stroke,
SkBlendMode = 3, antialias, shouldBorderScale)` (`0x2840e8a`). With a stroke it
builds `Painter::CreateRoundRectWith1PxBorderPainter`, without one
`CreateSolidRoundRectPainter`. `omnibox_frame.h` hands it the idle line colour
for the call from `RefreshBackground` only (return address `0x28409b6`; the
other caller is the popup's `TopBackgroundView::OnThemeChanged`).

Limits: the border is 1px (the painter's), not the kit's 2px; and the
background is rebuilt only on focus, hover and theme changes, so the skin's
switch shows on the omnibox at the next of those (switched from the panel,
the theme re-apply does it).

## Runtime verification

Throwaway Edge only: own `--user-data-dir` and LOCALAPPDATA under
`app/dll/out/re-cyber-<ticks>/`, selected and killed by that marker, a copy of
the DLL as `edgeplus-split-re-<ticks>.dll` whose unload event name was patched
to a private one (the session-wide `Local\edgeplus-split-unload` was being
signalled by others within seconds of each load), skin forced by channel code
908, PrintWindow captures in `app/dll/out/shots-re/`:

- `frame-vertical-dark.png`: vertical list, red frame on the selected row, teal
  on the idle rows, both 2px, following the cut corner
- `frame-horizontal-dark.png`, `final-horizontal.png`: horizontal strip
- `frame-vertical-skin-off.png`: skin off, Edge's own look
- `font-vertical-dark-zoom.png`: titles in Share Tech Mono
- `font-toggle-strip.png`: skin on, off (through the config), on again: frames
  and font follow live
- `omnibox-preconfig-zoom.png`: the omnibox's 1px teal frame

These captures read at about 40% brightness everywhere (white text comes out
`#646464`): the throwaway window was never in front, and its frame is captured
mid fade. The frame pixels are the exact colours at the same scale
(`#63101b` = `#f72944` x 0.4, `#003030` = `#007979` x 0.4).

Not driven: a light theme (no theme in the throwaway; the light colours are
unit-tested), hover, a split group's tabs, RTL.

## Next

- **Uppercase titles**: hook `views::Label::SetText` (`0x110a570`, slot
  `0x3a8`) for labels whose vtable is `TabTitle`'s (`0x118542c0`) and pass an
  uppercased copy (`LCMapStringEx(LCMAP_UPPERCASE)` over the UTF-16). A live
  switch then needs the original titles back: `EdgeTab::data` (`0x1194800`,
  vtable `+0x38`) holds them, at an offset still to read.
- **Letter-spacing**: not available. `gfx::RenderText` lays text out with no
  tracking setting; faking it would mean drawing the title ourselves.
- **Omnibox at 2px / near-sharp corners**: the 1px is
  `CreateRoundRectWith1PxBorderPainter`'s; 2px needs another painter (a paint
  hook, not recommended). The radius is `LocationBarView::EdgeComputeBorderRadius`
  (`0x2840f9e`, also `GetBorderRadius`), an `int` return, a safe hook if wanted.
- **Section-label amber / red accent stub**: no candidate in the browser chrome
  the skin touches (the vertical list has no section labels on this build);
  left out.
- `control.h` `SetSplitHooksEnabled(false)` re-enables the skin's outline hooks
  by name but not `SkRRect::setRectXY`, `IsLeftSplitTab` and `IsRightSplitTab`
  (found while testing, not changed here): with the split switched off, the
  vertical tab's cut may not show.
