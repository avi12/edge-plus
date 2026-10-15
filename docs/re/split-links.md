# A split pane's new-tab links

Target: `msedge.dll` 155.0.4283.45. All addresses are **RVAs**; their one home
in code is `app/native-split/src/offsets.h`.

The pane ⋯ menu's toggle "Open left links to the right", relabelled "Open
new-tab links in a new pane" (`pane-menu.md`), now decides where a link that
would open a **new tab** (`target=_blank`, no opener) goes, when it is clicked
in any pane of a split:

| toggle | the link opens |
|---|---|
| off | in the pane itself; no tab is made |
| on | in a new pane right next to that pane |

Code: `src/links.h`. Out of scope: modifier clicks, popups, and pages that
open a window to script it.

## The toggle

The toggle is command id 6 of `SplitWindowControlMenu`. Its value is one
`bool` per split, at **`SplitTabData+0x28`**:

- `IsLinkedModeEnabledForTab` (`0xa020860`) reads it.
- `SetLinkedModeStateForTab` (`0xa020880`) writes it, and the
  `edge_split_window.default_link_mode` pref when asked.

A **new** split takes its value from that pref. That includes every split
re-created by a grow, close or re-split. So `split_data.h` reads the split's
settings (its visual and this toggle) before it re-creates a split, and
writes the toggle back afterwards.

After storing the value, `SetLinkedModeStateForTab` calls
`UpdateLinkedModeState` (`0xa01ffc0`). That notifies the split's observers,
which wire Edge's two-pane link mode: they prepare a target pane, and with
three panes they navigated pane 1 to a new tab page. Under the new meaning the
toggle is just the stored `bool`, read when a link is clicked, so that call is
a no-op.

## Where a new-tab link goes in Edge

1. The renderer asks `RenderFrameHostImpl::CreateNewWindow` (`0x78df140`) for
   a window.
2. Once `ChromeContentBrowserClient::CanCreateWindow` (the popup blocker)
   allows it, Edge adds an asynchronous step,
   `WebContentsImpl::EdgeShouldCreateNewWindowAsync` (`0x79d9e10`). It asks the
   contents' delegate, vtable `+0x3c8`, which is
   **`BrowserWebContentsDelegate::ShouldOverrideNewWindowCreation`**
   (`0x9fb7960`). Its arguments: `this`, the source `WebContents*`, the opener
   frame, the disposition, a bool, and `CreateNewWindowParams*`.
3. If that returns true, no window is made.
   - Edge returns true when the split feature is on, the disposition is 3 or
     12, and the source is the split's active tab.
   - In that case `SplitWindowController` (vtable `+0x90`) handles the link.
     It is built for two panes: with the toggle off it keeps the link in its
     pane.
4. Otherwise `WebContentsImpl::CreateNewWindow` (`0x79cca00`) makes the
   contents. Then `BrowserWebContentsDelegate::AddNewContents` (`0x9fb5040`)
   puts them into the strip.

| `CreateNewWindowParams` field | offset |
|---|---|
| `opener_suppressed` | `+0x50` |
| disposition | `+0x78` |
| `target_url` | `+0x80` |

**`WindowOpenDisposition` values:** 3 is a new foreground tab, 4 a new
background tab, and 12 Edge's split foreground tab.

## The detours

| detour | does |
|---|---|
| `ShouldOverrideNewWindowCreation` | For a foreground-tab link with no opener from any split pane: **off** loads the URL in the source pane (`NavigationController::LoadURLParams` + `LoadURLWithParams`) and returns true; **on** returns false, so the tab gets made. Everything else goes to the original. |
| `AddNewContents` | With the toggle on, a foreground new-tab link (with a user gesture and no opener, checked with `WebContentsImpl::HasOpener`) goes in as a **background** tab: that puts it right after the split, and the active pane stays. The UI-thread channel (`kLinkPane`) then adds it as a pane right of the source pane. |
| `SplitWindowLinkModeNavigationThrottle::MaybeCreateThrottleFor` (`0x29d2d54`) | Never made (out pointer null). It sent the first pane's same-tab links to the last pane when the toggle was on. |
| `SplitWindowControllerImplV2::UpdateLinkedModeState` | A no-op, see above. |

## Crashes this exposed in grown splits

- **Edge's split metrics.** `split_tabs::LogSplitViewCreatedUKM` and
  `LogSplitViewUpdatedUKM` (`0xa23d3c4`, `0xa23d512`) each do
  `CHECK(ListTabs().size() == 2)`.
  - `Updated` runs from `UpdateTabInSplitImpl`, and whenever a split member's
    page becomes primary, i.e. it navigates.
  - So a page navigating in a split of three crashed Edge.
  - Any other split now skips them.
- **A memory leak.** `GetTabsAndIndicesInSplit` returns a vector that Edge
  allocates for the caller. `McvActiveSplitPanes` (every layout) never freed
  it. `edge::SplitMembers` now frees it with Edge's `operator delete`
  (`0x36b2e50`).

## Testing it from a script

**Page input is routed by the real cursor**, so a posted click lands wherever
the cursor happens to be. The test therefore uses the keyboard: the test page
focuses its link on load, and a posted Enter activates it with a user
gesture.

**The toggle is set from the profile**, not from the menu, because opening
the menu moves keyboard focus out of the page:
`Default/Preferences` = `{"edge_split_window":{"default_link_mode":true}}`.
