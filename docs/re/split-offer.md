# A new pane's tab offer

Target: `msedge.dll` 155.0.4283.45. All addresses are **RVAs**; their one home
in code is `app/native-split/src/edge/offsets.h`.

A pane added from the ⋯ d-pad or the toolbar's split menu opens on Edge's
split **starting page**, the "Tabs you've opened" page Edge's own split button
puts in its second pane. The user picks the pane's tab there, types an
address, or closes the pane with the page's X.

Code: `src/split/offer.h`.

## How Edge opens it

- `SplitWindowController::ToggleSplitWindow` (`0xa0230ee`) calls
  `edge::split_screen::HandleOpenSplitWindow` (`0xa027707`) when no split is
  open.
- That `Navigate()`s to `chrome::kEdgeUISplitWindowStartingPageURL`
  (`0x1216ed60`, `edge://split-window-starting-page`) with disposition 12,
  Edge's split foreground tab: a new tab, split with the active one.

**Ours** makes the page a background tab with `chrome::AddAndReturnTabAt`
(`0xc951100`) and grows the split with it, like any other added pane.
`AddAndReturnTabAt` takes the `BrowserWindowInterface*`, which is what
`BrowserView+0x600` holds (its `+0x30` is the tab strip). A `Browser*` would
need `+0x68`, the offset Edge itself adds in `HandleOpenSplitWindow`.

## The page

The page talks to `SplitWindowStartingUIHandler` over mojo. Its WebContents
is at `handler+0x38` (a `content::WebUI*`) and then `+0x68`.

| method | does |
|---|---|
| `GetTabs` (`0x9ab4950`) | lists every tab of the window but those `IsSplitWindowWebContents` (`0x29d5e22`) claims |
| `OpenTabInSplitWindow` (`0x9ab4c70`) | calls the controller's `OpenTabInCurrentSplitWindow` (vtable `+0xd0`) |
| `CloseSplitWindow` (`0x9ab5750`) | the X: if the page is the active contents, calls `HideSplitView(true)` (vtable `+0x20`) |
| `OpenMainTabInSplitWindow` (`0x9ab57e0`) | the "Left tab" card: loads the split's first member's URL into the page |

**Built for two panes.** Picking a tab and the X both act on the active
split's **last** member, because a two-pane split always keeps its starting
page there:

- `SplitWindowControllerImplV2::OpenTabInCurrentSplitWindow` (`0xa0213b0`)
  calls `UpdateTabInSplit(tsm, ListTabs().back(), chosen, 0)`. Flag 0
  replaces that member with the chosen tab and closes the member.
- `HideSplitView(true)` (`0xa0201f0`) closes `ListTabs().back()`.

**Removing a split** runs `SplitWindowControllerImplV2::OnSplitTabRemoved`
(`0xa022106`). If the removed split's last member is a starting page (judged
by its user data, `IsStartingPage(contents, false)`, `0xa023034`), it posts a
task: the lambda at `split_window_controller_impl_v2.cc:1313`, `RunOnce`
`0xa022e40`. The task loads the new tab page there, replacing the entry.

## Live updates

Open pages also hear about tab changes from `StartingPageNotifierImpl`, which
the controller keeps at `+0x78`. It is made by the first
`GetStartingPageNotifier`.

- `OnTabCreated` (`0xa028e9e`) and `OnTabUpdated` (`0xa0293b0`) send each page
  a tab. A created tab is added to the list; an update for a tab the page does
  not list is ignored.
- `OnTabClosing(this, WebContents*)` (`0xa0290ec`) sends each page
  `OnTabRemoved(tab id)`.

## The detours

They act only in a **grown** split (three or more members); a two-pane split
is Edge's own.

| detour | in a grown split |
|---|---|
| `OpenTabInCurrentSplitWindow` | replaces the active member, the starting page the user picked in |
| `CloseSplitWindow` | closes the page's own pane, posted (`kCloseOfferPane`) so the page's call returns first |
| `GetTabs` + `IsSplitWindowWebContents` | while the list is built, also claims every tab in a split, save the split's first member (the "Left tab" copy) |
| the `OnSplitTabRemoved` task | skips a page that is back in a split by the time it runs (see below) |

**Why the removal task needs a detour.** Growing a split, closing one of its
panes and a pick all remove the split and re-add its tabs in one go.

**Pane joins.** When a tab joins a grown split, `GrowWithTree` and the pick
call the notifier's `OnTabClosing` for it, so open pages drop its card.

## Known gaps

- **Dissolving a grown split with several pages open.** Edge turns only the
  last member's page into a new tab page; the others stay starting pages as
  plain tabs.
- **Tabs that leave a split.** They are not added back to the lists already
  open.

## Testing it from a script

- **Cards are page content**, routed by the real cursor, so a script parks the
  cursor on the card for the click (`contentclick.ps1` in the session
  scratchpad).
- **The page's X is page content too.**
- **The channel's `kMenuArrowBase + direction`** runs the same add as a d-pad
  arrow.
