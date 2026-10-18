# Edge's split view, from the binary

Why Edge+ splits a tab with its own windows instead of extending Edge's
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
- **Panes as tiled Edge windows.** Worked, but each pane was its own window,
  not a pane of the tab. Two measurements from it: Edge's bounds include a 7px
  invisible resize border on the left, right and bottom, and Edge will not make
  a tabbed window narrower than 516 or shorter than 150.

## What Edge+ does instead

A split tab shows the extension's split page, and each pane is a frame inside
it, on the user's own profile. The layout is a tree of row and column splits
(`extension/src/features/split-panes/layout.ts`), and the whole state - the
tree, every pane's address and the focused pane - lives in the tab's own
address after the `#`, so it survives reloads and restarts and the background
edits it with a same-document navigation that reloads no pane.

- **Frame refusals.** Most sites send X-Frame-Options or a CSP frame-ancestors
  directive. A declarativeNetRequest session rule removes both, for sub-frames
  of split tabs only; every other tab keeps its headers.
- **Cross-origin panes.** The split page cannot read a pane's address or drive
  its history, so a content script inside each pane reports its address and
  title and runs back, forward and reload on request (`lib/pane-bridge.ts`).
- **Any number of panes.** Nothing caps the tree. Adding several tabs at once
  halves the biggest pane on screen across its longer side for each one
  (`grow`), measured against the tab's real width and height, so twelve tabs
  land as a grid; `tile` rebuilds any count as even rows in reading order. The
  selection comes from the tab strip's own multi-select (`highlighted` tabs),
  since an extension cannot add items to Edge's tab context menu.
- **No reloads.** Moving an iframe in the DOM reloads it, so panes render in a
  stable order positioned by CSS from the layout, and a frame's address is set
  once rather than bound.
