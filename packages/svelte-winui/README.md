# svelte-winui

WinUI 3 controls as Svelte components: native Windows apps written in Svelte,
the way React Native writes native apps in React. Every control is WinUI's own,
so the app looks and behaves like Windows: light and dark follow Windows, the
controls animate and are accessible as WinUI makes them, and acrylic is a real
backdrop. No browser engine is involved; there is no DOM.

```svelte
<script lang="ts">
  import { StackPanel, TextBlock, ToggleSwitch } from "svelte-winui/components";

  let isOn = $state(true);
</script>

<StackPanel padding={24} spacing={12}>
  <TextBlock text="Hello from WinUI" fontSize={24} />
  <ToggleSwitch bind:isOn header="Lights" />
</StackPanel>
```

- **Every WinUI 3 control** (167 of them, in the table below) is generated from
  WinUI's own metadata, with typed props, typed events and `bind:`-able
  properties. The package follows WinUI instead of a hand-picked subset.
- **Svelte's own compiler and runtime**, through its custom-renderer API.
  Runes, context, `{#if}`, `{#each}`, snippets, `svelte/reactivity` and stores
  work as they do on the web.
- **Hot reload** while you develop: save a `.svelte` file and the open window
  changes in place.
- **Framework-agnostic.** The window runs in a Node process (with a worker for
  WinUI's UI thread) that something else starts: a Tauri core, an Electron
  main process, or you from a terminal. The host and the window talk in JSON
  lines (or a `MessagePort` in-process).
- **Windows only.** Windows 10 version 1809 (build 17763) and Windows 11, on
  the Windows App SDK (WinUI 3) through [dynwinrt](https://github.com/microsoft/dynwinrt).

Built for [Edge+](../../README.md), whose settings panel is written with it
(`app/panel-tauri`).

> **Status: not published yet.** The package name is a working name, the
> custom-renderer API it needs is a Svelte pull request that has not shipped
> ([sveltejs/svelte#18042](https://github.com/sveltejs/svelte/pull/18042)), and
> `package.json` still says `"private": true`. What is described here is what
> the repository's tests and the [hello example](examples/hello) run.

## Requirements

| | |
| --- | --- |
| OS | Windows 10 1809 (17763) or newer, Windows 11 |
| Node | 24 or newer |
| Windows App SDK runtime | 2.2, installed on the machine. A missing runtime makes `initWindowsAppSdk` throw `WindowsAppRuntimeMissing`; the package does not install it (Edge+ ships the installer and offers it) |
| Svelte | the build with the custom-renderer API: `svelte@https://pkg.pr.new/svelte@18042` until it is released |
| Vite | 8 (for the dev server and hot reload) |

## Install

```sh
pnpm add svelte-winui svelte@https://pkg.pr.new/svelte@18042 @microsoft/dynwinrt
pnpm add -D vite @microsoft/winappcli @microsoft/dynwinrt-codegen
pnpm exec svelte-winui setup
pnpm install
```

The controls are drawn from WinUI **bindings your app generates for itself**:
the package imports them as `#winui/*`, which its `package.json` maps to your
`@winapp/bindings` package, a folder `winapp` writes to `.winapp/bindings`.
`svelte-winui setup` does everything that needs:

1. writes `winapp.yaml` (the Windows App SDK 2.2 and the tools `winapp` needs to
   generate bindings) if you have none;
2. adds the classes the package uses to `package.json`'s
   `winapp.jsBindings.additionalWinmds` (the ones you listed stay), and the
   dependency `"@winapp/bindings": "link:.winapp/bindings"`;
3. runs `winapp restore` (fetches the Windows App SDK, writes the Windows App
   SDK bootstrapper to `.winapp/bin`) and `winapp node generate-bindings`
   (writes `.winapp/bindings`).

The first run downloads the SDK and generates thousands of binding files; it
can take ten minutes or more. It is not repeated until the class list or the
SDK version changes. The last `pnpm install` links `.winapp/bindings` into
`node_modules`. Add `.winapp` to `.gitignore`.

Options: `svelte-winui setup [directory] [--dry-run] [--winapp <command>]`.
`--dry-run` writes the files and does not run `winapp`. A class of your own that
the window uses (`Microsoft.UI.Windowing.DisplayArea`, say) goes in the same
`additionalWinmds` list, and you import it from `#winui/...` as the package does
(add `"imports": { "#winui/*": "@winapp/bindings/*" }` to your `package.json`).

## Quick start

The files below are the [hello example](examples/hello), run for real by
`examples/hello/run.ps1` (it flips the switch through UI Automation and edits
`App.svelte` to check hot reload).

`vite.config.ts` is one line of plugin:

<!-- hello/vite.config.ts:start -->

```ts
import { svelteWinui } from "svelte-winui/vite";
import { defineConfig } from "vite";

export default defineConfig({
  plugins: [svelteWinui()]
});
```

<!-- hello/vite.config.ts:end -->

`App.svelte` is the window's content. `bind:isOn` works on the component like any
Svelte binding:

<!-- hello/App.svelte:start -->

```svelte
<script lang="ts">
  import { StackPanel, TextBlock, ToggleSwitch } from "svelte-winui/components";

  let isOn = $state(true);
</script>

<StackPanel padding={24} spacing={12}>
  <TextBlock text="Hello from WinUI" fontSize={24} />
  <ToggleSwitch bind:isOn header="Lights" />
  <TextBlock text={isOn ? "The lights are on" : "The lights are off"} />
</StackPanel>
```

<!-- hello/App.svelte:end -->

`window.ts` runs on the UI thread. WinUI's message loop never gives its thread
back, so `runOnUiThread` (the package's `/thread` entry) is a worker's first
call: it enters the apartment before anything creates a WinRT object. The
window's modules come from the host's dev server through the module runner, and
a timer takes the server's hot updates in (Node's own timers never fire while
WinUI holds the thread):

<!-- hello/window.ts:start -->

```ts
import type { MessagePort } from "node:worker_threads";
import { workerData } from "node:worker_threads";
import { createWindowModuleRunner } from "svelte-winui/runner";
import { runOnUiThread } from "svelte-winui/thread";
import type * as WindowModule from "svelte-winui/window";

// The window: WinUI's thread. Its modules come from the host's dev server, and
// a timer takes the server's hot updates in, since WinUI holds this thread.

const receiveMilliseconds = 30;
const windowSize = {
  width: 480,
  height: 320
};

function isModulesPort(value: unknown): value is { modules: MessagePort } {
  return typeof value === "object" && value !== null && "modules" in value;
}

await runOnUiThread(async () => {
  const data: unknown = workerData;
  if (!isModulesPort(data)) {
    throw new Error("the host passed no port to its dev server");
  }

  // Everything goes through the runner, so the window and the components
  // share one renderer.
  const { runner, receive } = createWindowModuleRunner(data.modules);
  const { createWindow, runWindow, startTimer } = await runner.import<typeof WindowModule>("svelte-winui/window");
  const { default: App } = await runner.import("/App.svelte");
  await runWindow({
    onStart() {
      startTimer({
        milliseconds: receiveMilliseconds,
        onTick: receive
      });
      createWindow({
        component: App,
        props: {},
        title: "Hello",
        size: windowSize,
        // Set for an unattended run: the window opens without taking focus.
        isBackground: process.env["SVELTE_WINUI_BACKGROUND"] === "1"
      });
    }
  });
});
```

<!-- hello/window.ts:end -->

`main.ts` is the host: it starts the Windows App SDK, the dev server and the
window:

<!-- hello/main.ts:start -->

```ts
import path from "node:path";
import { MessageChannel, Worker } from "node:worker_threads";
import { initWindowsAppSdk } from "svelte-winui/bootstrap";
import { startModuleServer } from "svelte-winui/vite";

// The host: it starts the Windows App SDK, serves the window's modules from a
// Vite dev server (so a saved .svelte file updates the open window) and starts
// the window, which runs on a worker because WinUI keeps its thread.
//
//   node main.ts        (from the folder `svelte-winui setup` was run in)

const windowsAppSdkMajor = 2;
const windowsAppSdkMinor = 2;

initWindowsAppSdk({
  major: windowsAppSdkMajor,
  minor: windowsAppSdkMinor,
  folders: [path.join(process.cwd(), ".winapp", "bin", process.arch)]
});

const modules = new MessageChannel();
await startModuleServer({
  root: import.meta.dirname,
  configFile: path.join(import.meta.dirname, "vite.config.ts"),
  port: modules.port1
});
const worker = new Worker(new URL("./window.ts", import.meta.url), {
  workerData: {
    modules: modules.port2
  },
  transferList: [modules.port2]
});
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exitCode = 1;
});
worker.on("exit", code => {
  process.exit(process.exitCode ?? code);
});
```

<!-- hello/main.ts:end -->

Run it from the folder `setup` was run in:

```sh
node main.ts
```

Edit the text in `App.svelte` and save: the window follows within a moment.
Set `SVELTE_WINUI_BACKGROUND=1` to open the window without taking focus (for
scripts and captures).

State changes need no `flushSync`: the window's loop is started with
`Application.startScheduled`, so Node drains microtasks after each WinUI
callback and Svelte's own flush runs ([experiment](examples/scheduling/README.md)).

## Host integration

The window is a process (or a worker) that something else owns. What the host
sends and receives is the same everywhere: JSON values, one per line on stdio,
or the same objects through a `MessagePort`. State the host owns reaches the
window as one deeply reactive object, and the window answers with actions; see
[`src/state/README.md`](src/state/README.md) for the wire protocol, `hostState`
and `send`.

**Tauri** (what Edge+ does). The Rust core starts the window as a child process
(a sidecar: your `main.ts`, or a bundled executable with Node inside) with its
stdin and stdout piped, writes `{"type":"snapshot","name":"view","value":{...}}`
and later `patch` lines, and reads `action` lines back, answering each with a
`reply`. In the window, `createStdioTransport()` and `reserveStdout()` (so a
stray `console.log` goes to stderr and cannot corrupt the protocol) are the
other end. `app/panel-tauri/ui/main.ts` is the working example, with the core in
`app/panel-tauri/src-tauri`.

**Electron.** The main process starts the window with
`utilityProcess.fork("main.js")` (or `child_process.spawn` of Node) and speaks
the same messages over the process's stdio or a `MessagePort`; the main
process is where the state lives and where `diff(previous, next)` from
`svelte-winui/state/host` makes the patches. The Windows App SDK is started by
the window's own process, so the Electron renderer is not involved.
This path has the same protocol as Tauri's but has not been run in this
repository.

**Node.** The simplest host is your own script: it starts the Windows App SDK
and the window as `main.ts` above does, and keeps its state in plain objects.
`examples/counter` is a host in Node that owns `{ count, history }`, sends it
with `diff()`, and answers the window's actions over a `MessagePort`
(`examples/counter/run.ps1` clicks its buttons through UI Automation).

## Controls

Every concrete WinUI 3 control is a component in `svelte-winui/components`. A
component has the properties of its WinUI class and of every class it inherits
from, as camelCase props (`isOn`, `horizontalAlignment`), its events as `on...`
props (`onToggled`), and `bind:` on a property that has a matching change event.
The table shows what each class adds; `Bindable` includes the inherited
bindables. `Children` says the component takes content (its WinUI content
property: `Content`, `Child`, `Children`, `Items`...) as children.

<!-- controls:start -->

| Component | Namespace | Props it adds | Bindable | Children |
| --- | --- | --- | --- | --- |
| `AnimatedIcon` | Controls | `fallbackIconSource`, `mirroredWhenRightToLeft`, `source` |  |  |
| `AnimatedVisualPlayer` | Controls | `animationOptimization`, `autoPlay`, `fallbackContent`, `playbackRate`, `source`, `stretch` |  |  |
| `AnnotatedScrollBar` | Controls | `detailLabelTemplate`, `labels`, `labelTemplate`, `smallChange` |  |  |
| `AppBar` | Controls | `closedDisplayMode`, `isOpen`, `isSticky`, `lightDismissOverlayMode` | `isOpen` | yes |
| `AppBarButton` | Controls | `dynamicOverflowOrder`, `icon`, `isCompact`, `keyboardAcceleratorTextOverride`, `label`, `labelPosition` |  | yes |
| `AppBarElementContainer` | Controls | `dynamicOverflowOrder`, `isCompact` |  | yes |
| `AppBarSeparator` | Controls | `dynamicOverflowOrder`, `isCompact` |  |  |
| `AppBarToggleButton` | Controls | `dynamicOverflowOrder`, `icon`, `isCompact`, `keyboardAcceleratorTextOverride`, `label`, `labelPosition` | `isChecked` | yes |
| `AutoSuggestBox` | Controls | `autoMaximizeSuggestionArea`, `description`, `header`, `isSuggestionListOpen`, `lightDismissOverlayMode`, `maxSuggestionListHeight`, `placeholderText`, `queryIcon`, +4 more | `text` | yes |
| `BitmapIcon` | Controls | `showAsMonochrome`, `uriSource` |  |  |
| `Bold` | Documents |  |  | yes |
| `Border` | Controls | `background`, `backgroundSizing`, `backgroundTransition`, `borderBrush`, `borderThickness`, `child`, `childTransitions`, `cornerRadius`, +1 more |  | yes |
| `BreadcrumbBar` | Controls | `itemsSource`, `itemTemplate` |  |  |
| `BreadcrumbBarItem` | Controls |  |  | yes |
| `Button` | Controls | `flyout` |  | yes |
| `CalendarDatePicker` | Controls | `calendarIdentifier`, `calendarViewStyle`, `date`, `dateFormat`, `dayOfWeekFormat`, `description`, `displayMode`, `firstDayOfWeek`, +10 more | `date` |  |
| `CalendarPanel` | Controls.Primitives |  |  | yes |
| `CalendarView` | Controls | `blackoutBackground`, `blackoutForeground`, `blackoutStrikethroughBrush`, `calendarIdentifier`, `calendarItemBackground`, `calendarItemBorderBrush`, `calendarItemBorderThickness`, `calendarItemCornerRadius`, +65 more |  |  |
| `CalendarViewDayItem` | Controls | `isBlackout` |  |  |
| `Canvas` | Controls |  |  | yes |
| `CarouselPanel` | Controls.Primitives | `canHorizontallyScroll`, `canVerticallyScroll`, `scrollOwner` |  | yes |
| `CheckBox` | Controls |  | `isChecked` | yes |
| `ColorPicker` | Controls | `color`, `colorSpectrumComponents`, `colorSpectrumShape`, `isAlphaEnabled`, `isAlphaSliderVisible`, `isAlphaTextInputVisible`, `isColorChannelTextInputVisible`, `isColorPreviewVisible`, +12 more | `color` |  |
| `ColorPickerSlider` | Controls.Primitives | `colorChannel` | `value` |  |
| `ColorSpectrum` | Controls.Primitives | `color`, `components`, `hsvColor`, `maxHue`, `maxSaturation`, `maxValue`, `minHue`, `minSaturation`, +2 more | `color` |  |
| `ComboBox` | Controls | `description`, `header`, `headerTemplate`, `isDropDownOpen`, `isEditable`, `isTextSearchEnabled`, `lightDismissOverlayMode`, `maxDropDownHeight`, +5 more | `selectedIndex`, `selectedItem`, `selectedValue`, `isDropDownOpen` | yes |
| `ComboBoxItem` | Controls |  |  | yes |
| `CommandBar` | Controls | `commandBarOverflowPresenterStyle`, `defaultLabelPosition`, `isDynamicOverflowEnabled`, `overflowButtonVisibility` | `isOpen` | yes |
| `CommandBarFlyoutCommandBar` | Controls.Primitives | `systemBackdrop` | `isOpen` | yes |
| `CommandBarOverflowPresenter` | Controls |  |  | yes |
| `ContentControl` | Controls | `content`, `contentTemplate`, `contentTemplateSelector`, `contentTransitions` |  | yes |
| `ContentDialog` | Controls | `closeButtonCommand`, `closeButtonCommandParameter`, `closeButtonStyle`, `closeButtonText`, `defaultButton`, `fullSizeDesired`, `isPrimaryButtonEnabled`, `isSecondaryButtonEnabled`, +10 more |  | yes |
| `ContentPresenter` | Controls | `background`, `backgroundSizing`, `backgroundTransition`, `borderBrush`, `borderThickness`, `characterSpacing`, `content`, `contentTemplate`, +19 more |  | yes |
| `DatePicker` | Controls | `calendarIdentifier`, `date`, `dayFormat`, `dayVisible`, `header`, `headerTemplate`, `lightDismissOverlayMode`, `maxYear`, +7 more | `date`, `selectedDate` | yes |
| `DropDownButton` | Controls |  |  | yes |
| `Ellipse` | Shapes |  |  |  |
| `Expander` | Controls | `expandDirection`, `header`, `headerTemplate`, `headerTemplateSelector`, `isExpanded` | `isExpanded` | yes |
| `FlipView` | Controls | `useTouchAnimationsForAllNavigation` | `selectedIndex`, `selectedItem`, `selectedValue` | yes |
| `FlipViewItem` | Controls |  |  | yes |
| `FlyoutPresenter` | Controls | `isDefaultShadowEnabled` |  | yes |
| `FontIcon` | Controls | `fontFamily`, `fontSize`, `fontStyle`, `fontWeight`, `glyph`, `isTextScaleFactorEnabled`, `mirroredWhenRightToLeft` |  |  |
| `Frame` | Controls | `cacheSize`, `isNavigationStackEnabled`, `sourcePageType` |  | yes |
| `Glyphs` | Documents | `colorFontPaletteIndex`, `fill`, `fontRenderingEmSize`, `fontUri`, `indices`, `isColorFontEnabled`, `originX`, `originY`, +2 more |  |  |
| `Grid` | Controls | `backgroundSizing`, `borderBrush`, `borderThickness`, `columnSpacing`, `cornerRadius`, `padding`, `rowSpacing` |  | yes |
| `GridView` | Controls |  | `selectedIndex`, `selectedItem`, `selectedValue` | yes |
| `GridViewHeaderItem` | Controls |  |  | yes |
| `GridViewItem` | Controls |  |  | yes |
| `GridViewItemPresenter` | Controls.Primitives | `checkBrush`, `checkHintBrush`, `checkSelectingBrush`, `contentMargin`, `disabledOpacity`, `dragBackground`, `dragForeground`, `dragOpacity`, +14 more |  | yes |
| `GroupItem` | Controls |  |  | yes |
| `Hub` | Controls | `defaultSectionIndex`, `header`, `headerTemplate`, `isActiveView`, `isZoomedInView`, `orientation`, `semanticZoomOwner` |  | yes |
| `HubSection` | Controls | `contentTemplate`, `header`, `headerTemplate`, `isHeaderInteractive` |  | yes |
| `Hyperlink` | Documents | `elementSoundMode`, `isTabStop`, `navigateUri`, `tabIndex`, `underlineStyle`, `xYFocusDown`, `xYFocusDownNavigationStrategy`, `xYFocusLeft`, +5 more |  | yes |
| `HyperlinkButton` | Controls | `navigateUri` |  | yes |
| `IconSourceElement` | Controls | `iconSource` |  | yes |
| `Image` | Controls | `nineGrid`, `source`, `stretch` |  |  |
| `ImageIcon` | Controls | `source` |  |  |
| `InfoBadge` | Controls | `iconSource`, `value` |  |  |
| `InfoBar` | Controls | `actionButton`, `closeButtonCommand`, `closeButtonCommandParameter`, `closeButtonStyle`, `content`, `contentTemplate`, `iconSource`, `isClosable`, +5 more | `isOpen` |  |
| `InfoBarPanel` | Controls.Primitives | `horizontalOrientationPadding`, `verticalOrientationPadding` |  | yes |
| `InlineUIContainer` | Documents | `child` |  | yes |
| `Italic` | Documents |  |  | yes |
| `ItemContainer` | Controls | `child`, `isSelected` |  |  |
| `ItemsControl` | Controls | `displayMemberPath`, `groupStyleSelector`, `itemContainerStyle`, `itemContainerStyleSelector`, `itemContainerTransitions`, `itemsPanel`, `itemsSource`, `itemTemplate`, +1 more |  | yes |
| `ItemsPresenter` | Controls | `footer`, `footerTemplate`, `footerTransitions`, `header`, `headerTemplate`, `headerTransitions`, `padding` |  |  |
| `ItemsRepeater` | Controls | `background`, `horizontalCacheLength`, `itemsSource`, `itemTemplate`, `itemTransitionProvider`, `layout`, `verticalCacheLength` |  |  |
| `ItemsRepeaterScrollHost` | Controls | `horizontalAnchorRatio`, `scrollViewer`, `verticalAnchorRatio` |  |  |
| `ItemsStackPanel` | Controls | `areStickyGroupHeadersEnabled`, `cacheLength`, `groupHeaderPlacement`, `groupPadding`, `itemsUpdatingScrollMode`, `orientation` |  | yes |
| `ItemsView` | Controls | `isItemInvokedEnabled`, `itemsSource`, `itemTemplate`, `itemTransitionProvider`, `layout`, `selectionMode`, `verticalScrollController` |  |  |
| `ItemsWrapGrid` | Controls | `areStickyGroupHeadersEnabled`, `cacheLength`, `groupHeaderPlacement`, `groupPadding`, `itemHeight`, `itemWidth`, `maximumRowsOrColumns`, `orientation` |  | yes |
| `Line` | Shapes | `x1`, `x2`, `y1`, `y2` |  |  |
| `LineBreak` | Documents |  |  |  |
| `ListBox` | Controls | `selectionMode`, `singleSelectionFollowsFocus` | `selectedIndex`, `selectedItem`, `selectedValue` | yes |
| `ListBoxItem` | Controls |  |  | yes |
| `ListView` | Controls |  | `selectedIndex`, `selectedItem`, `selectedValue` | yes |
| `ListViewHeaderItem` | Controls |  |  | yes |
| `ListViewItem` | Controls |  |  | yes |
| `ListViewItemPresenter` | Controls.Primitives | `checkBoxBorderBrush`, `checkBoxBrush`, `checkBoxCornerRadius`, `checkBoxDisabledBorderBrush`, `checkBoxDisabledBrush`, `checkBoxPointerOverBorderBrush`, `checkBoxPointerOverBrush`, `checkBoxPressedBorderBrush`, +51 more |  | yes |
| `MapControl` | Controls | `center`, `interactiveControlsVisible`, `layers`, `mapServiceToken`, `zoomLevel` |  |  |
| `MediaPlayerElement` | Controls | `areTransportControlsEnabled`, `autoPlay`, `isFullWindow`, `posterSource`, `source`, `stretch`, `transportControls` |  |  |
| `MediaPlayerPresenter` | Controls | `isFullWindow`, `mediaPlayer`, `stretch` |  |  |
| `MediaTransportControls` | Controls | `fastPlayFallbackBehaviour`, `isCompact`, `isFastForwardButtonVisible`, `isFastForwardEnabled`, `isFastRewindButtonVisible`, `isFastRewindEnabled`, `isNextTrackButtonVisible`, `isPlaybackRateButtonVisible`, +17 more |  |  |
| `MenuBar` | Controls |  |  |  |
| `MenuBarItem` | Controls | `title` |  |  |
| `MenuFlyoutItem` | Controls | `command`, `commandParameter`, `icon`, `keyboardAcceleratorTextOverride`, `text` |  | yes |
| `MenuFlyoutPresenter` | Controls | `isDefaultShadowEnabled`, `systemBackdrop` |  | yes |
| `MenuFlyoutSeparator` | Controls |  |  |  |
| `MenuFlyoutSubItem` | Controls | `icon`, `text` |  | yes |
| `MonochromaticOverlayPresenter` | Controls.Primitives | `replacementColor`, `sourceElement` |  | yes |
| `NavigationView` | Controls | `alwaysShowHeader`, `autoSuggestBox`, `compactModeThresholdWidth`, `compactPaneLength`, `contentOverlay`, `expandedModeThresholdWidth`, `footerMenuItemsSource`, `header`, +24 more | `isPaneOpen`, `selectedItem` | yes |
| `NavigationViewItem` | Controls | `hasUnrealizedChildren`, `icon`, `infoBadge`, `isChildSelected`, `isExpanded`, `menuItemsSource`, `selectsOnInvoked` |  | yes |
| `NavigationViewItemHeader` | Controls |  |  | yes |
| `NavigationViewItemPresenter` | Controls.Primitives | `icon`, `infoBadge` |  | yes |
| `NavigationViewItemSeparator` | Controls |  |  | yes |
| `NumberBox` | Controls | `acceptsExpression`, `description`, `header`, `headerTemplate`, `isWrapEnabled`, `largeChange`, `maximum`, `minimum`, +11 more | `value` |  |
| `Page` | Controls | `bottomAppBar`, `navigationCacheMode`, `topAppBar` |  | yes |
| `Paragraph` | Documents | `textIndent` |  | yes |
| `ParallaxView` | Controls | `child`, `horizontalShift`, `horizontalSourceEndOffset`, `horizontalSourceOffsetKind`, `horizontalSourceStartOffset`, `isHorizontalShiftClamped`, `isVerticalShiftClamped`, `maxHorizontalShiftRatio`, +6 more |  |  |
| `PasswordBox` | Controls | `description`, `header`, `headerTemplate`, `inputScope`, `isPasswordRevealButtonEnabled`, `maxLength`, `password`, `passwordChar`, +6 more | `password` |  |
| `Path` | Shapes | `data` |  |  |
| `PathIcon` | Controls | `data` |  |  |
| `PersonPicture` | Controls | `badgeGlyph`, `badgeImageSource`, `badgeNumber`, `badgeText`, `contact`, `displayName`, `initials`, `isGroup`, +2 more |  |  |
| `PipsPager` | Controls | `maxVisiblePips`, `nextButtonStyle`, `nextButtonVisibility`, `normalPipStyle`, `numberOfPages`, `orientation`, `previousButtonStyle`, `previousButtonVisibility`, +3 more |  |  |
| `Pivot` | Controls | `headerFocusVisualPlacement`, `headerTemplate`, `isHeaderItemsCarouselEnabled`, `isLocked`, `leftHeader`, `leftHeaderTemplate`, `rightHeader`, `rightHeaderTemplate`, +4 more | `selectedIndex`, `selectedItem` | yes |
| `PivotHeaderPanel` | Controls.Primitives |  |  | yes |
| `PivotItem` | Controls | `header` |  | yes |
| `PivotPanel` | Controls.Primitives |  |  | yes |
| `Polygon` | Shapes | `fillRule`, `points` |  |  |
| `Polyline` | Shapes | `fillRule`, `points` |  |  |
| `Popup` | Controls.Primitives | `child`, `childTransitions`, `desiredPlacement`, `horizontalOffset`, `isLightDismissEnabled`, `isOpen`, `lightDismissOverlayMode`, `placementTarget`, +3 more | `isOpen` | yes |
| `ProgressBar` | Controls | `isIndeterminate`, `showError`, `showPaused` | `value` |  |
| `ProgressRing` | Controls | `isActive`, `isIndeterminate`, `maximum`, `minimum`, `value` |  |  |
| `RadioButton` | Controls | `groupName` | `isChecked` | yes |
| `RadioButtons` | Controls | `header`, `headerTemplate`, `itemsSource`, `itemTemplate`, `maxColumns`, `selectedIndex`, `selectedItem` | `selectedIndex`, `selectedItem` |  |
| `RadioMenuFlyoutItem` | Controls | `groupName`, `isChecked` |  | yes |
| `RatingControl` | Controls | `caption`, `initialSetValue`, `isClearEnabled`, `isReadOnly`, `itemInfo`, `maxRating`, `placeholderValue`, `value` | `value` |  |
| `Rectangle` | Shapes | `radiusX`, `radiusY` |  |  |
| `RefreshContainer` | Controls | `pullDirection`, `visualizer` |  | yes |
| `RefreshVisualizer` | Controls | `content`, `orientation` |  |  |
| `RelativePanel` | Controls | `backgroundSizing`, `borderBrush`, `borderThickness`, `cornerRadius`, `padding` |  | yes |
| `RepeatButton` | Controls.Primitives | `delay`, `interval` |  | yes |
| `RevealListViewItemPresenter` | Controls |  |  | yes |
| `RichEditBox` | Controls | `acceptsReturn`, `characterCasing`, `clipboardCopyFormat`, `description`, `desiredCandidateWindowAlignment`, `disabledFormattingAccelerators`, `header`, `headerTemplate`, +15 more |  |  |
| `RichTextBlock` | Controls | `characterSpacing`, `fontFamily`, `fontSize`, `fontStretch`, `fontStyle`, `fontWeight`, `foreground`, `horizontalTextAlignment`, +18 more |  | yes |
| `RichTextBlockOverflow` | Controls | `maxLines`, `overflowContentTarget`, `padding` |  |  |
| `Run` | Documents | `flowDirection`, `text` |  | yes |
| `ScrollBar` | Controls.Primitives | `indicatorMode`, `orientation`, `viewportSize` | `value` |  |
| `ScrollContentPresenter` | Controls | `canContentRenderOutsideBounds`, `canHorizontallyScroll`, `canVerticallyScroll`, `scrollOwner`, `sizesContentToTemplatedParent` |  | yes |
| `ScrollPresenter` | Controls.Primitives | `background`, `content`, `contentOrientation`, `horizontalAnchorRatio`, `horizontalScrollChainMode`, `horizontalScrollController`, `horizontalScrollMode`, `horizontalScrollRailMode`, +10 more |  |  |
| `ScrollView` | Controls | `content`, `contentOrientation`, `horizontalAnchorRatio`, `horizontalScrollBarVisibility`, `horizontalScrollChainMode`, `horizontalScrollMode`, `horizontalScrollRailMode`, `ignoredInputKinds`, +9 more |  |  |
| `ScrollViewer` | Controls | `bringIntoViewOnFocusChange`, `canContentRenderOutsideBounds`, `horizontalAnchorRatio`, `horizontalScrollBarVisibility`, `horizontalScrollMode`, `horizontalSnapPointsAlignment`, `horizontalSnapPointsType`, `isDeferredScrollingEnabled`, +20 more |  | yes |
| `SelectorBar` | Controls | `selectedItem` | `selectedItem` |  |
| `SelectorBarItem` | Controls | `icon`, `text` |  |  |
| `SemanticZoom` | Controls | `canChangeViews`, `isZoomedInViewActive`, `isZoomOutButtonEnabled`, `zoomedInView`, `zoomedOutView` |  | yes |
| `Slider` | Controls | `header`, `headerTemplate`, `intermediateValue`, `isDirectionReversed`, `isThumbToolTipEnabled`, `orientation`, `snapsTo`, `stepFrequency`, +3 more | `value` |  |
| `Span` | Documents | `inlines` |  | yes |
| `SplitButton` | Controls | `command`, `commandParameter`, `flyout` |  | yes |
| `SplitMenuFlyoutItem` | Controls | `subMenuItemStyle`, `subMenuPresenterStyle` |  | yes |
| `SplitView` | Controls | `compactPaneLength`, `content`, `displayMode`, `isPaneOpen`, `lightDismissOverlayMode`, `openPaneLength`, `pane`, `paneBackground`, +1 more | `isPaneOpen` | yes |
| `StackPanel` | Controls | `areScrollSnapPointsRegular`, `backgroundSizing`, `borderBrush`, `borderThickness`, `cornerRadius`, `orientation`, `padding`, `spacing` |  | yes |
| `SwipeControl` | Controls | `bottomItems`, `leftItems`, `rightItems`, `topItems` |  | yes |
| `SymbolIcon` | Controls | `symbol` |  |  |
| `SystemBackdropElement` | Controls | `cornerRadius`, `systemBackdrop` |  |  |
| `TabView` | Controls | `addTabButtonCommand`, `addTabButtonCommandParameter`, `allowDropTabs`, `canDragTabs`, `canReorderTabs`, `canTearOutTabs`, `closeButtonOverlayMode`, `isAddTabButtonVisible`, +10 more | `selectedIndex`, `selectedItem` |  |
| `TabViewItem` | Controls | `header`, `headerTemplate`, `iconSource`, `isClosable` |  | yes |
| `TabViewListView` | Controls.Primitives |  | `selectedIndex`, `selectedItem`, `selectedValue` | yes |
| `TeachingTip` | Controls | `actionButtonCommand`, `actionButtonCommandParameter`, `actionButtonContent`, `actionButtonStyle`, `closeButtonCommand`, `closeButtonCommandParameter`, `closeButtonContent`, `closeButtonStyle`, +12 more | `isOpen` | yes |
| `TextBlock` | Controls | `characterSpacing`, `fontFamily`, `fontSize`, `fontStretch`, `fontStyle`, `fontWeight`, `foreground`, `horizontalTextAlignment`, +17 more |  | yes |
| `TextBox` | Controls | `acceptsReturn`, `characterCasing`, `description`, `desiredCandidateWindowAlignment`, `header`, `headerTemplate`, `horizontalTextAlignment`, `inputScope`, +18 more | `text` |  |
| `Thumb` | Controls.Primitives |  |  |  |
| `TickBar` | Controls.Primitives | `fill` |  |  |
| `TimePicker` | Controls | `clockIdentifier`, `header`, `headerTemplate`, `lightDismissOverlayMode`, `minuteIncrement`, `selectedTime`, `time` | `selectedTime`, `time` | yes |
| `TitleBar` | Controls | `autoRefreshDragRegions`, `content`, `iconSource`, `isBackButtonEnabled`, `isBackButtonVisible`, `isPaneToggleButtonVisible`, `leftHeader`, `rightHeader`, +2 more |  |  |
| `ToggleButton` | Controls.Primitives | `isChecked`, `isThreeState` | `isChecked` | yes |
| `ToggleMenuFlyoutItem` | Controls | `isChecked` |  | yes |
| `ToggleSplitButton` | Controls | `isChecked` | `isChecked` | yes |
| `ToggleSwitch` | Controls | `header`, `headerTemplate`, `isOn`, `offContent`, `offContentTemplate`, `onContent`, `onContentTemplate` | `isOn` | yes |
| `ToolTip` | Controls | `horizontalOffset`, `isOpen`, `placement`, `placementRect`, `placementTarget`, `verticalOffset` | `isOpen` | yes |
| `TreeView` | Controls | `canDragItems`, `canReorderItems`, `itemContainerStyle`, `itemContainerStyleSelector`, `itemContainerTransitions`, `itemsSource`, `itemTemplate`, `itemTemplateSelector`, +3 more | `selectedItem` |  |
| `TreeViewItem` | Controls | `collapsedGlyph`, `expandedGlyph`, `glyphBrush`, `glyphOpacity`, `glyphSize`, `hasUnrealizedChildren`, `isExpanded`, `itemsSource` |  | yes |
| `TreeViewList` | Controls |  | `selectedIndex`, `selectedItem`, `selectedValue` | yes |
| `TwoPaneView` | Controls | `minTallModeHeight`, `minWideModeWidth`, `pane1`, `pane1Length`, `pane2`, `pane2Length`, `panePriority`, `tallModeConfiguration`, +1 more |  |  |
| `Underline` | Documents |  |  | yes |
| `UserControl` | Controls | `content` |  | yes |
| `VariableSizedWrapGrid` | Controls | `horizontalChildrenAlignment`, `itemHeight`, `itemWidth`, `maximumRowsOrColumns`, `orientation`, `verticalChildrenAlignment` |  | yes |
| `Viewbox` | Controls | `child`, `stretch`, `stretchDirection` |  | yes |
| `VirtualizingStackPanel` | Controls | `areScrollSnapPointsRegular`, `orientation` |  | yes |
| `WrapGrid` | Controls | `horizontalChildrenAlignment`, `itemHeight`, `itemWidth`, `maximumRowsOrColumns`, `orientation`, `verticalChildrenAlignment` |  | yes |

<!-- controls:end -->

How a value is written:

- An enum prop takes the member's name (`horizontalAlignment="Center"`) or its
  number.
- A thickness takes `8`, `[8, 4]`, `[left, top, right, bottom]` or a CSS-order
  string; a corner radius a number or an array; a grid length `"Auto"`, `"*"`,
  `"2*"` or pixels.
- A brush takes a color string or a WinUI theme resource name, so light and dark
  stay WinUI's.
- A property that takes any object (`header`, `content`) takes a string, which is
  boxed for it, or a control.
- Attached properties are plain props: `gridRow`, `gridColumn`, `gridRowSpan`,
  `gridColumnSpan`, `canvasLeft`, `canvasTop`, `canvasZIndex`,
  `automationName`, `automationHelpText`, `toolTip`. Any other:
  `attached={[Owner, "Property", value]}`.
- A `ref` to the native control: `{@attach control(native => ...)}` from
  `svelte-winui` hands your code the WinUI object (typed from your bindings);
  `withControl(StackPanel, panel => ...)` checks its class first.

The raw elements the components are made of work too: `<stack-panel spacing={8}>`,
`<text-block>`, `grid.column={1}` for an attached property; they need
`registerControls(...)` (the components register the generated bindings by being
imported). Components are the supported way; see [API.md](API.md) for the raw
forms and the renderer's functions.

## Styling with CSS

`<style>` blocks are translated at build time into WinUI `Style`s and applied
by class, so a card is `class="card"` and a rule, as on the web. Layout
properties map onto WinUI's panels. A declaration with no WinUI meaning is a
build error with its file and line; nothing is silently ignored.
[`src/css/API.md`](src/css/API.md) is the interface; what maps:

| CSS | WinUI |
| --- | --- |
| `margin`, `padding`, `border-width`, `border-color`, `border-radius` | `Margin`, `Padding`, `BorderThickness`, `BorderBrush`, `CornerRadius` |
| `color`, `background-color`, `opacity` | `Foreground`, `Background`, `Opacity` |
| `width`, `height`, `min-*`, `max-*` | `Width`, `Height`, `MinWidth`... |
| `font-size`, `font-weight`, `font-family`, `font-style`, `line-height`, `letter-spacing`, `text-align`, `text-wrap`, `text-overflow`, `text-decoration` | the `TextBlock` and `Control` font and text properties |
| `gap`, `row-gap`, `column-gap` | `Spacing` (a stack), `RowSpacing` and `ColumnSpacing` (a grid) |
| `flex-direction`, `display: flex` on a stack, `align-self`, `justify-self` | `Orientation`, `VerticalAlignment`, `HorizontalAlignment` |
| `grid-template-columns`, `grid-template-rows`, `grid-column`, `grid-row` | `ColumnDefinitions`, `RowDefinitions`, `Grid.Column`... |
| `display: none`, `visibility`, `pointer-events`, `z-index`, `overflow` | `Visibility`, `IsHitTestVisible`, `Canvas.ZIndex`, the scroll bar visibilities |
| `translate`, `scale`, `rotate`, `transition` | the UIElement transforms and their implicit transitions |
| `var(--card-background-fill-color-default)` | `{ThemeResource CardBackgroundFillColorDefault}`: WinUI's own resources, so light and dark follow Windows |
| `.class`, `type`, `:hover`, `:focus`, `:active`, `:root`, `:where()` | class and type matching, the control's events, custom properties |

## State

Svelte's own state tools work unchanged (runes, context, `SvelteMap`, stores).
The package adds `svelte-winui/state`: `createHostChannel(transport)`,
`hostState<View>(channel, "view")` and `channel.send(action)`, for state a Tauri
core, an Electron main process or a script owns. A host written in JavaScript
imports `svelte-winui/state/host` (`diff`, the transports, the message types),
which loads in plain Node. [`src/state/README.md`](src/state/README.md) has the
protocol and the tests.

## Entry points

| Specifier | What |
| --- | --- |
| `svelte-winui` | `control`, `withControl`, `mountControl`, `registerControls`, `dispatch` / `flush`, the value helpers |
| `svelte-winui/components` | every control as a component, and its props type |
| `svelte-winui/state` | the window's side of host state |
| `svelte-winui/state/host` | a JavaScript host's side: `diff`, transports, message types |
| `svelte-winui/vite` | `svelteWinui()` for `vite.config.ts`; `startModuleServer()` for hot reload |
| `svelte-winui/runner` | `createWindowModuleRunner()`, the window thread's side of the module server |
| `svelte-winui/window` | `runWindow`, `createWindow`, `startTimer`, `exitApplication` |
| `svelte-winui/thread` | `runOnUiThread`, the first call of the UI thread's worker |
| `svelte-winui/bootstrap` | `initWindowsAppSdk`, for the host's main thread |
| `svelte-winui/testing` | Node module hooks that run `.svelte` files from source, for tests |
| `svelte-winui/compile`, `/host`, `/keyed-list` | the compiler hook, the renderer, a keyed collection sync |
| `svelte-winui/css`, `/css/vite`, `/css/sheet-module` | the CSS runtime, its Vite plugin and the sheet module compiler ([`src/css/API.md`](src/css/API.md)) |

The published package ships `dist/`: the TypeScript compiled with its
declarations, and the Svelte files as source (with `types` and `svelte`
conditions on `exports`), which your Vite compiles with the custom renderer.

## Limitations

Stated plainly, so nobody finds them by surprise:

- **Windows only**, and only Windows 10 1809 and newer. The Windows App SDK
  runtime must be installed; the package does not install it.
- **Needs an unreleased Svelte.** The custom-renderer API is a pull request;
  until it ships, the package works with the pkg.pr.new build named above.
- **`bind:` works on components, not on raw elements.** `<ToggleSwitch bind:isOn>`
  is supported; `<toggle-switch bind:is-on>` is not. Use the component, or
  `{@attach control(...)}` and subscribe to the control's event yourself.
- **A property whose name starts with "on"** (`ToggleSwitch.OnContent`) is a
  prop on components, but on a raw element Svelte takes the attribute for an
  event: write it as `On-content="On"` or `on-content={() => "On"}`.
- **No `:hover` or `:active` miracles on templated controls.** A pseudo-class
  applies a Style when the control reports the event, but WinUI's own template
  states (a Button's pointer-over and pressed brushes) take over from a Style's
  setters while they are active, and a control that handles an event itself (a
  Button handles `PointerPressed`) never reports it, so `:active` does nothing
  there. Override the control's theme resources instead.
- **Mica is not available yet.** `createWindow({ backdrop })` takes `"acrylic"`
  and `"none"`; Mica needs `MicaBackdrop` in the app's bindings and the option
  to name it, which is not done.
- **CSS is WinUI's subset.** No `position`, `float`, `flex-grow`, `box-shadow`,
  `filter`, `transform`, `animation`, descendant, child or sibling selectors,
  nesting or `@keyframes`: each is a build error that says what to use instead.
  `@media (prefers-color-scheme)` can declare custom properties only (a Style
  cannot depend on the scheme).
- **A hot update to a component that is not self-contained** (a changed `.ts`
  module no component accepts) cannot be applied in place; the dev server says
  so and the window must be restarted. A hot update that cannot compile is
  reported and the window keeps its code.
- **A component's new root control after a hot update** replaces the old one in
  a panel's children, a border's child or a content control's content. A host
  that holds the root somewhere else must listen with `mounted.onRootChange`
  (`createWindow` does).
- **Electron is untested** here (the protocol is shared with Tauri's, which is).
- **Bindings are yours to generate**, and take minutes the first time
  (`svelte-winui setup`).

## Development

In this repository (a pnpm workspace; run these in `packages/svelte-winui`):

| | |
| --- | --- |
| `pnpm test` | the unit tests, the generator's tests, and the mount test: every component made and linked in a real WinUI window that opens off screen and never takes focus |
| `pnpm lint` | oxlint, eslint, `tsc`, fallow and the check that the generated files are current |
| `pnpm gen:components` | regenerates the components, their metadata, the class list `setup` writes and the tables and quick start in this README from WinUI's metadata (`pnpm restore` in `app/panel-tauri` first) |
| `pnpm build` | `dist/`: what is published (`pnpm pack` shows the tarball) |
| `pwsh examples/hello/run.ps1` | the hello example in a background window, checked through UI Automation (`-Edit` also checks hot reload) |

[DESIGN.md](DESIGN.md) is the architecture and [API.md](API.md) the core API.

## License

[Apache License 2.0](../../LICENSE). Keep the [NOTICE](../../NOTICE) file with
copies you distribute (section 4(d) of the license): it credits the author.
