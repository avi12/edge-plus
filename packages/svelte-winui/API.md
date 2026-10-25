# svelte-winui: the core's public API

What `src/renderer`, `src/vite` and `src/window` export. Names are fixed once
this file lands; the other parts code against them. Everything is TypeScript
source: `package.json` `exports` point at `.ts` files, so an app in the
monorepo needs no build step.

## Entry points

| Specifier | File | What |
|---|---|---|
| `svelte-winui` | `src/index.ts` | the attachment, `mountControl`, `registerControls`, `dispatch` / `flush`; the components add their exports here |
| `svelte-winui/host` | `src/renderer/host.ts` | the custom renderer a compiled component imports (default export); not for app code |
| `svelte-winui/vite` | `src/vite/index.ts` | `svelteWinui()` Vite plugin, `startModuleServer()` |
| `svelte-winui/runner` | `src/vite/runner.ts` | `createWindowModuleRunner()`, the window thread's side of the module server |
| `svelte-winui/compile` | `src/vite/compile.ts` | `compileSvelte()` for bundlers (esbuild) and tests |
| `svelte-winui/testing` | `src/vite/loader.ts` | `svelteHooks()`: Node module hooks that run `.svelte` files with a chosen renderer |
| `svelte-winui/window` | `src/window/index.ts` | `initWindowsAppSdk()`, `runOnUiThread()`, `runWindow()`, `createWindow()` |

The WinUI bindings come from the app: the package imports `#winui/*`, which its
`package.json` maps to the `@winapp/bindings` package that `winapp node
generate-bindings` writes. The app depends on it
(`"@winapp/bindings": "link:.winapp/bindings"`) and lists every class the
window uses in `winapp.jsBindings`. A class the package itself needs
(`Application`, `Window`, `DispatcherQueue`, `Grid`, `ColumnDefinition`,
`RowDefinition`, `PropertyValue`, `GridUnitType`, `XamlControlsResources`) must
be in that list.

## Renderer (`svelte-winui`)

```ts
// {@attach control<ToggleSwitch>(toggle => { ...; return () => cleanup })}
// Hands the element's native control to the handler once it is made. The
// handler may return a cleanup; Svelte runs it when the element goes or the
// attachment re-runs. Effects created inside the handler belong to the
// attachment, so they end with it (a hot swap included). Nothing is called
// for a `<host>` that holds no control.
function control<Control extends object>(
  handler: (control: Control) => void | (() => void)
): Attachment;

// {@attach withControl(StackPanel, panel => ...)}: the same, checked with
// instanceof (the control is skipped when it is another class).
function withControl<Control>(
  type: abstract new (...values: never[]) => Control,
  use: (control: Control) => void
): Attachment;

interface Mounted<Props> {
  readonly root: FrameworkElement;       // the component's first control
  set(changes: Partial<Props>): void;    // changes props, flushes at once
  unmount(): void;                       // Svelte's unmount, then releases hosted controls
}
function mountControl<Props extends object>(component: Component<Props>, initial: Props): Mounted<Props>;

// The classes (and enums) a tag, an enum-valued attribute or `attached` may
// name, as the bindings' exports: `registerControls(winui)` where `winui` is
// the app's one import site of the bindings. Later calls add to earlier ones.
function registerControls(controls: Record<string, unknown>): void;

// What `class="..."` does on an element, until the CSS runtime takes it over:
// the app decides (Edge+ looks the name up in its XAML styles).
type ClassHandler = (control: UIElement, value: string) => void;
function setClassHandler(handler: ClassHandler): void;

// Raw kebab elements (<stack-panel>, <toggle-switch on-content="...">) keep
// working: `on-content` is the property OnContent, because only a lower-case
// `on` + name with no dash in it is an event (`ontoggled={...}`).
```

Raw-element rules (unchanged): kebab-case tag = WinUI class, kebab attribute =
camelCase property, `grid.column={1}` = attached property, `columns` / `rows`
on a grid, `<host element={control} />` places a control made elsewhere.

## Scheduling (`svelte-winui`)

```ts
// Runs `work` on the UI thread's DispatcherQueue, then flush(). The way state
// that arrives off the dispatcher (a message from the host) reaches the UI.
function dispatch(work: () => void): void;

// Svelte's flushSync(), then every onFlush listener. Call it after changing
// state from code that is not an event handler.
function flush(): void;

// For other parts (the CSS runtime) that must act after Svelte has applied a
// batch. Returns the function that removes the listener.
function onFlush(listener: () => void): () => void;
```

## Vite (`svelte-winui/vite`)

```ts
// The one line in an app's vite.config: wraps @sveltejs/vite-plugin-svelte
// with the custom renderer, client generation, css "external" (the CSS part
// takes the <style> blocks over) and the "browser" resolve condition.
function svelteWinui(options?: SvelteWinuiOptions): Plugin[];

interface SvelteWinuiOptions {
  // The module a component imports its renderer from. Default: svelte-winui/host.
  renderer?: string;
}

// The pure part of the above, for tests.
function winuiPluginOptions(options?: SvelteWinuiOptions): { svelte: SvelteOptions; conditions: string[] };

// Hot reload inside the window process: a Vite dev server with no HTTP
// server, talking to the module runner over a MessagePort pair.
function startModuleServer(options: {
  root: string;
  port: MessagePort;                       // the runner's counterpart
  isNative: (file: string) => boolean;     // modules Node loads itself (the bindings' import site)
  renderer?: string;
}): Promise<ViteDevServer>;
```

A compile error in a `.svelte` file is printed on the host process's stderr as
`file:line:column: message` plus the code frame, and the window keeps running.

## Window (`svelte-winui/window`)

```ts
// Windows App SDK bootstrap: finds Microsoft.WindowsAppRuntime.Bootstrap.dll
// in `folders`, sets WINAPPSDK_BOOTSTRAP_DLL_PATH and initialises it. Throws
// WindowsAppRuntimeMissing when the runtime is not installed.
function initWindowsAppSdk(options: { major: number; minor: number; folders: string[] }): void;

// In the worker that owns the UI thread: enters the single-threaded
// apartment first, so everything `main` loads may create WinRT objects, then
// awaits main and ends the process.
function runOnUiThread(main: () => Promise<void>): Promise<never>;

// Starts WinUI on this thread (the caller is the UI-thread worker):
// Application, WinUI's own resources, then `onStart` on the UI thread. The
// returned promise settles when the application exits.
function runWindow(options: { onStart: () => void }): Promise<void>;

// A window around a component or a control.
function createWindow(options: {
  title?: string;
  content: Component | UIElement;
  props?: object;                  // for a component
  size?: { width: number; height: number };
  backdrop?: "acrylic" | "mica" | "none";
  extendsIntoTitleBar?: boolean;
  background?: boolean;            // open without activating (tests, captures)
}): AppWindow;

interface AppWindow {
  readonly window: Window;
  readonly mounted: Mounted<object> | null;
  close(): void;
  onClosed(listener: () => void): void;
}
```

Who starts the process is the app's business (Tauri sidecar, Electron utility
process, plain Node). A transport (JSON lines on stdio, a MessagePort) is not
in these files: it is `src/state`'s, which uses `dispatch` to put host
messages on the UI thread.
