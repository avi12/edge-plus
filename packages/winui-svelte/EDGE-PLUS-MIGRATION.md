# Edge+ on the package's `<ListView>` and `<Window>`

How `app/panel-tauri/ui/window/own-theme-list.ts` became a `<ListView>`
(`OwnThemeList.svelte`) and `shell.ts` a `<Window>` (`Shell.svelte`). **Both
are done** on the converted Edge+ (commits "the own themes list is a ListView
of the package" and "the shell is a Window of the package"), pixel-identical
to the window before them (0 differing pixels, default and states views, with
and without the Edge hint and build notice, from source and from the window
kit). The tables below say what moved where. Two differences from the first
proof on `edgeplus-migration-proof`: there is no `Shell` facade class (the
panel's state holds the status line, the open window and the pickers, and
`session.ts` mounts `Shell.svelte` itself), and `onBoundsChanged` also reports
the bounds the window opens with.

## Before anything: regenerate the bindings

The package now asks for more classes (`MicaBackdrop` and `MicaKind` among
them; `gen:components` merged them into `app/panel-tauri/package.json`'s
`winapp.jsBindings`). `.winapp/bindings` is not tracked, so after merging:

```
cd app/panel-tauri
pnpm bindings                          # about 6 minutes; writes .winapp/bindings
cd ../../packages/winui-svelte
pnpm gen:components:check              # must be silent: no "bindings missing"
```

## The list: `own-theme-list.ts` to `OwnThemeList.svelte`

New `OwnThemeList.svelte` (a component, mounted by `OwnThemeList` exactly as
`EdgeThemeList` mounts `EdgeThemeList.svelte`; `view` is the mounted root,
which `ThemingBody.svelte` still places with `<host element={ownThemes} />`; once
the body is converted, drop the class and put `<OwnThemeList {rows} ...>` there):

```svelte
<ListView
  items={rows} key={row => row.id} itemName={row => row.name}
  reorderable keyboardReorder {onReorder}
  selectionMode="None" isItemClickEnabled={false}
  itemContainerStyle={themeListItemStyle()}
  automationName={labels.themeListName} automationHelpText={labels.reorderHelp}
  visibility={rows.length > 0 ? "Visible" : "Collapsed"}
  onControl={view => motion.animateItems(view, motion.listItems)}
>
  {#snippet item(row)}
    <ThemeRow theme={row} onRemove={() => onRemove(row.id)} />
  {/snippet}
</ListView>
```

| In `own-theme-list.ts` | Now |
|---|---|
| `new ListView()`, `selectionMode`, `isItemClickEnabled` | `<ListView selectionMode="None" isItemClickEnabled={false}>` |
| `canReorderItems`, `canDragItems`, `allowDrop` = true | `reorderable` |
| `itemContainerStyle = themeListItemStyle()` | the same prop; `styles.ts` stays (its XAML style is the one thing CSS cannot say) |
| `AutomationProperties.setName/HelpText(view, ...)` | `automationName`, `automationHelpText` props |
| `rowSlotTemplate`, `itemTemplate`, `itemsSource = keys` (the vector of boxed keys) | the component's (`handWrittenComponents`); delete the XAML string |
| `update(rows)`: `syncKeyed` into the key vector, `visibility` | `items={rows}`; `visibility` prop |
| `fill` on `ContainerContentChanging`: `themeRow(row, ...)` into the slot, `setName(container, name)` | the `item` snippet and `itemName`; `theme-row.ts`'s `themeRow()` (a `mountControl`), `rowKey`, `idOf`, `nameOf`, `keyText` are no longer used: delete them |
| `onDragItemsCompleted`: `resetRowStates`, `actions.reorder(order())` | `onReorder={ids => ...}` (the component resets the rows' visual state first) |
| `onPreviewKeyDown` Alt+Up/Down, `focusedIndex`, `moveKeepingFocus`, `actions.move` | `keyboardReorder`; it moves the item in the list, calls `onReorder` and keeps the focus on it |
| `motion.animateItems(this.view, ...)` | `onControl={view => motion.animateItems(view, ...)}` |

What stays app code: `motion.ts` (the Animation-effects switch). Two changes in
it: `animateItems` takes a `ListViewBase` (add it to `winui.ts`), and returns
what un-registers the list (`onControl` may return a cleanup, which the package
runs when the list goes; before, a list stayed in the module's array for good).

`OwnThemeList` becomes a few lines (see the branch): `mountControl(OwnThemeListView,
{ rows, labels, onRemove, onReorder })`, `update(rows)` is `set({ rows })`.
`OwnThemeActions` loses `move`: the keyboard reorder now reports the new order
like a drag, so `theming-card.ts` has one `reorderThemes` action for both and
the core's `moveTheme` action is unused (remove it from `protocol.rs`,
regenerate `protocol.g.ts`, and the Rust handler, when convenient).

Behaviour that differs, on purpose:

- **The key is the id, not `id + name + badges`.** A theme whose name or badges
  change keeps its row, which shows the new values (`ThemeRow.svelte` reads
  `theme` reactively); before, the row was replaced and its add/remove transition
  played again.
- **Rows are ended when their container is recycled** (before, none were: a
  long list leaked one component per row shown).
- Rows are mounted outside the list's component tree: `getContext` in
  `ThemeRow.svelte` would not see the page's context (it uses none).
- The list keeps the user's order after `onReorder` until `items` changes: the
  core answers `reorderThemes` with a View in that order, which finds the list
  already there.

`ListHandle` (`bind:handle`) also gives `move(key, direction)`, `scrollToKey`
and `focus(key)`, for buttons or shortcuts of the app's own.

## The window: `shell.ts` to `Shell.svelte`

New `Shell.svelte` is a `<Window>` root; `shell.ts` stays as a thin facade so
`session.ts` changes in three lines. The facade mounts it with `mountControl`
and passes the page (`PanelView.root`) as `content`, placed in the window with
`<host element={content} />`.

| In `shell.ts` | Now (`<Window ...>`) |
|---|---|
| `systemBackdrop = new DesktopAcrylicBackdrop()` | `backdrop="acrylic"` (also `"mica"`, `"micaAlt"`, `"none"`) |
| `extendsContentIntoTitleBar = true`, `setTitleBar(bar)`, the row layout `Grid` | the `{#snippet titleBar()}`: shown above the content, set as the title bar, extends the content (no layout Grid) |
| `title`, `appWindow.setIcon(view.icon)` | `title`, `icon` props |
| `titleBar()`: a `Grid` of an `Image`, the name, the status `TextBlock` | the snippet's markup: `<grid height={40} padding="14,0,0,0" columns="Auto,Auto,*">`, `<Image source={fileUrl}>`, two `<text-block class="caption" / class="card-description">` |
| `showStatus`: text, opacity 1, timer, opacity 0 | `statusLine` and `isStatusShown` props set by the facade; the timer stays in the facade |
| `fixWidth`: `scaleOf`, min/max width = 560, min height, `isMaximizable = false`, `moveAndResize(placement)`, else `resize` and centre | `width={560} minWidth={560} maxWidth={560} height={640} minHeight={420} isMaximizable={false}`, `placement={placement && { x, y, height }}` (no width: the props' width applies), centred by default |
| `onActivated` (not Deactivated): `motion.refresh(); link.refresh()` | `onActiveChanged={isActive => ...}` |
| `appWindow.onChanged` to `link.reportBounds` | `onBoundsChanged={reportBounds}` (only moves and resizes) |
| `onClosed(exitApplication)` | `onClosed` prop (`ShellLink.onClosed`) |
| `activate()` / `showInBackground()` | `activate={!isBackground}`; `session.ts` reads `EDGEPLUS_WINDOW_BACKGROUND` and passes `isBackground` to `new Shell(...)` |
| `bringBack()`: `presenter.restore()` + `activate()` | `handle.bringToFront()` |
| `close()` | `handle.close()` |
| `joinAppUserModel(view.appUserModelId)` in `session.ts` | `appUserModelId` prop |
| `FolderPicker(window.appWindow.id)` | `handle.appWindow.id`; the picker code stays in the facade |

`handle` reaches the facade through a prop callback: `Shell.svelte` binds
`handle` and calls `onOpen(handle)`; the facade keeps it for `bringBack`,
`close` and the pickers.

Other changes the move needs: `PanelHost.window` goes (nothing in `PanelView`
used it); `native.ts` is deleted (`scaleOf` and `joinAppUserModel` are the
package's now; its eslint exception goes with it); `session.ts` loses the
`joinAppUserModel` call and `shell.onClosed/activate/showInBackground`.

What stays app code: `motion.ts`, `session.ts`'s message handling, the
pickers, the status timer, `styles.ts`, the View and Action types, and which
environment variable means "background".

## Checking it

```
powershell.exe -NoProfile -ExecutionPolicy Bypass -File <scratchpad>\cmp\hcap.ps1 -Panel <worktree>\app\panel-tauri -Out default.png
powershell.exe ... -Out states.png -View <scratchpad>\cmp\view-states.json
python -I px.py bg-ref-default-1.png default.png      # differing px: 0
python -I px.py bg-ref-states.png states.png          # differing px: 0
```

and, in the package, `pnpm test:windows` (real windows: 1000 rows, a grid, every
backdrop, props followed live) and `examples/lists/run.ps1` (a posted Alt+Down
reorders).
