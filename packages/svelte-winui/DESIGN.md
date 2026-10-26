# svelte-winui — design

Svelte for native Windows apps, the way React Native is React for native
mobile apps: an app imports WinUI 3's controls as ordinary Svelte components,
styles them with CSS in `<style>` blocks, keeps its state in runes, and gets
Vite hot reload while it develops. Every control is WinUI's own, so the app
looks and behaves like Windows (light and dark follow Windows live, acrylic
and Mica are real backdrops, accessibility is UI Automation's).

```svelte
<script lang="ts">
  import { StackPanel, TextBlock, ToggleSwitch } from "svelte-winui";
  let isOn = $state(true);
</script>

<StackPanel class="card">
  <TextBlock text="Split into any grid" />
  <ToggleSwitch bind:isOn />
</StackPanel>

<style>
  .card { padding: 12px 16px; gap: 2px; background: var(--card-background-fill-color-default); }
</style>
```

(`svelte-winui` is a working name.)

## Requirements

- **Windows 10 (17763+) and 11 only.** It runs on the Windows App SDK
  (WinUI 3), through dynwinrt.
- **No app framework assumed.** It works under Tauri, Electron and plain
  Node: the window runs in a Node process (or worker) that the host starts,
  and talks to the host in JSON lines (or a MessagePort in-process).
- **Every WinUI 3 control** is a component, generated from WinUI's metadata
  (WinMD), with typed props, events and bindable properties, so the library
  follows the framework as it is, not a hand-picked subset.
- **Svelte's own compiler and runtime**, through Svelte's custom-renderer API
  (sveltejs/svelte#18042; installed from pkg.pr.new until it ships). Nothing
  Svelte does is re-implemented.
- Generic: nothing in the package knows about any app (Edge+ is its first
  user and its proving ground, in `app/panel-tauri`).

## Layout

| Folder | What | Owner in the first round |
|---|---|---|
| `src/renderer/` | the custom renderer (the node tree, the WinUI host that turns elements into controls), `mount` | renderer |
| `src/vite/` | the Vite plugin (compile options, custom renderer, CSS hook) and the module server/runner that gives hot reload inside the window process | renderer |
| `src/window/` | starting WinUI: the bootstrap, the UI thread, Application and Window | renderer |
| `src/components/` | the generated components and their index, plus `scripts/codegen/` that generates them from the WinMD | components |
| `src/css/` | `<style>` blocks to WinUI Styles: the compile step and the runtime that applies them | css |
| `src/state/` | scheduling (updates flushed on WinUI's dispatcher), `hostState()` and `send()` over a transport | state |

## Contracts between the parts

### Component to renderer

A generated component renders one renderer element named after its WinUI
class in kebab case (`<toggle-switch>`), and talks to its control directly
through an attachment the renderer provides:

- `{@attach control(handler)}`: `handler(control)` gets the element's native
  control (typed, from the bindings) and may return a cleanup. Generated
  components set properties and subscribe to events in it, typed against the
  bindings, so no property or event name goes through string mapping.
  Properties use `$effect`s inside the attachment so each re-runs only when its
  prop changes.
- Children: `{@render children?.()}` inside the element; the renderer places
  child controls where the control holds them (Panel.Children, Border.Child,
  ContentControl.Content, ItemsControl.Items, a TextBlock's Inlines/Text), as
  the WinUI content property names it.
- `class` and `style` on a component are passed to the element unchanged; the
  renderer hands them to the CSS runtime (below).

### Props

- Every dependency property of the class and its bases, as camelCase
  (`isOn`, `horizontalAlignment`, `onContent`). An enum prop takes the member
  name (`"Center"`) or the value. A Thickness takes `8`, `[8, 4]`,
  `[left, top, right, bottom]` or a CSS-order string. Brushes take a theme
  resource name or a color.
- A property with a matching change event (`IsOn`/`Toggled`,
  `Text`/`TextChanged`, `SelectedIndex`/`SelectionChanged`, `Value`/
  `ValueChanged`, …) is `$bindable()`.
- Events as `onToggled={(sender, args) => …}`, typed. A property whose name
  starts with "on" (ToggleSwitch.OnContent) stays a property: components are
  not raw elements, so Svelte's `on*` rule for elements does not apply.
- Attached properties of the common owners as plain props:
  `gridRow`, `gridColumn`, `gridRowSpan`, `gridColumnSpan`, `canvasLeft`,
  `canvasTop`, `canvasZIndex`, `automationName`, `automationHelpText`,
  `toolTip`; anything else through `attached={[Owner, "Property", value]}`.

- A grid's `columns="Auto,*,Auto"` and `rows="Auto,Auto"` (lengths separated
  by commas) are hand-written props: WinUI's definition collections are
  read-only, so `src/components/extensions/grid.ts` adds them and the
  generator mixes them in (`extendedClasses` in `scripts/codegen/tables.ts`).

### CSS

The Vite plugin compiles each component's `<style>` (Svelte scopes it) into
WinUI Styles: one per (element type, class combination), with
`{ThemeResource}` setters for `var(--name)` so light and dark stay WinUI's.
The runtime registers them and the renderer applies the Style when an
element's classes change. Layout properties map to WinUI's panels (gap to
Spacing, grid-template-columns to ColumnDefinitions, …). A declaration with no
WinUI meaning fails the build with its line.

### State

Svelte's runes, context, `svelte/reactivity` and stores work unchanged. The
package adds: updates scheduled on WinUI's DispatcherQueue (app code never
calls `flushSync`), and `hostState<T>()` / `send(action)` over a transport
(JSON lines on stdio by default), so state the host owns appears as one
`$state` object in the window.

## Testing

- Pure parts (tree, CSS translation, codegen tables, state channel) run under
  `node --test` without WinUI.
- Every generated component is mounted at least once in a real WinUI window
  run in the background (off screen, never activated), so a control the
  generator gets wrong fails a test.
- Edge+'s window (`app/panel-tauri`) is checked pixel by pixel against its
  references after every change (0 differing pixels).
