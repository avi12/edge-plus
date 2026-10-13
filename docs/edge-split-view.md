# Edge's split view, from the binary

Why Fix Edge splits a tab with its own windows instead of extending Edge's
split view. Read off `msedge.dll` 155.0.4283.45 (353 MB, no symbols) with REA
and a string sweep of the image, on 2026-10-13.

## What Edge ships

Edge 155 carries two split implementations side by side:

- **Edge's own "Split Screen"** - `chrome\browser\ui\views\edge_split_window\`
  (`split_window_controller_impl.cc`, `split_window_controller_impl_v2.cc`,
  `split_window_container_view.h`), gated by the flag "Microsoft Edge Split
  Screen" and the prefs `edge_split_window.*`.
- **Chromium's "side by side"** - `MultiContentsView`,
  `MultiContentsResizeArea`, `SplitTabView`, `SplitTabMenuModel`, with Edge's
  migration shim `edge_split_window\migration_upstream\edge_multi_contents_resize_area.h`.

## Why it stops at two

The model and the UI name exactly two slots, start and end, with one ratio:

| string in the binary | what it says |
|---|---|
| `SplitTabMenuModel::kCloseStartTabMenuItem` / `kCloseEndTabMenuItem` | a split has a start tab and an end tab |
| `SplitTabSwapMenuModel::kSwapStartTabMenuItem` / `kSwapEndTabMenuItem` | the only rearrangement is swapping the two |
| `SplitTabMenuModel::kReversePositionMenuItem` | ditto |
| `splitTabsButtonAccNameEnabledLeft/Right/Top/Bottom` | the toolbar button knows two sides per orientation |
| `Cannot create split view with non-adjacent tabs.` | a split is a pair of neighbouring tabs |
| `Stacked Split Views` / `SplitViewHorizontal` - "Whether split views can be arranged in a stacked layout." | the only layout flag stacks the same two panes |

So there is no count to patch: the two-pane limit is the shape of the classes
themselves. More panes would mean new `views::View` subclasses and web views
built inside the browser process, against unexported, unsymbolized C++ whose
layouts change every four weeks.

## What was tried and dropped

- **In-process DLL** (the any-stt approach). Hot-swappable loading is easy; the
  N-pane UI is not, for the reason above.
- **Panes inside the app** (WebView2). Looks like one window, but a separate
  profile: no Edge sign-in, extensions or open tabs.

## What Fix Edge does instead

Each pane is a real Edge window tiled over the area the tab's window had. The
extension moves tabs between windows (Edge's supported way to do it), and the
layout is a tree of row and column splits (`src/features/split-panes/layout.ts`).
Two Windows facts shape the geometry, both measured:

- **Invisible resize borders.** Edge's bounds include a 7px invisible border on
  the left, right and bottom; panes that abut by bounds show a 14px seam.
  `toWindowBounds` grows each pane by the border on every side it shares.
- **Minimum window size.** Edge will not make a tabbed window narrower than 516
  or shorter than 150 (popup windows: 179 wide). Too-small bounds passed to
  `windows.create` are ignored outright, and `windows.update` clamps them. The
  popup only offers splits that fit.
