# Reloading one pane of a grown split

Target: `msedge.dll` 156.0.4314.42. All addresses are **RVAs**; their one home in
code is `app/native-split/src/edge/offsets.h`.

The toolbar's reload button, F5 and Ctrl+R all end in
`chrome::Reload` / `ReloadBypassingCache` (`0xa2b47c3`, `0xa2b4b59`), thin
wrappers over `chrome::(anonymous)::ReloadInternal(Browser*,
WindowOpenDisposition, bool bypass_cache)` (`0xa2b4827`).

`ReloadInternal` collects the tabs to reload: the active tab alone, unless the
selection (`TabStripModel+0x88` entries) holds more than `active->IsSplit() + 1`
tabs, in which case every selected tab. A split of two has both members selected
(2 is not more than 2), so it reloads the active pane only. In a split of three
or more all members are selected, the count passes, and **every pane reloads**.
Each tab goes through `NavigationControllerImpl::Reload` (`0x7940fd0`).

The fix is a detour on `ReloadInternal` (`src/edge/hooks.h`,
`h_reload_internal`): for the current-tab disposition in a split of three or
more, reload the active tab's page alone through `edge::ReloadPage` (normal, or
bypassing the cache); everything else runs Edge's original.

Verified on a throwaway Edge with a local page counting its loads: before, one
Refresh reloaded all three panes; after, only the active pane.
