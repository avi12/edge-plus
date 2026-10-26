# Edge+

Live feature switches for Microsoft Edge (see `README.md`):

- `app/native-split/` — C++20 DLL injected into Edge's browser process that grows
  Edge's native split view to N panes; pinned to one `msedge.dll` build
  (`src/edge/offsets.h`), reverse-engineered in `docs/re/`
- `app/panel-tauri/` — the panel that toggles each feature and injects the DLL:
  a Rust core (every Win32 call in its `edge-native` crate) and a WinUI 3
  window drawn from TypeScript
- `app/installer/` — the MSI (WiX 5 through its NuGet SDK) that installs the
  published exe under Program Files

These rules are `avi/youtube-time-manager`'s, carried to C++ the way
`avi/any-stt` carries them, and overridden only where C++ has a stronger
convention of its own. They exist to make the code **readable at a glance** by a
reader fluent in code but not in Edge's internals.

# Every language: `if` statements

1. **Always use curly braces — including one-liners — with the body on the next
   line.** Never `if (cond) doThing();`, and never collapse a block onto one line
   as `if (cond) { doThing(); }`. The same holds for `else`, `for`, `while`,
   `do` and `case` bodies. Braces are K&R: `{` stays on the `if` / `else` line,
   and `else` / `else if` keep the `} else {` shape.

   ```cpp
   // no
   if (!mcv) return;

   // yes
   if (!mcv) {
     return;
   }
   ```

2. **A complex condition is extracted into a descriptively-named boolean.** That
   means compound conditions (`&&`, `||`), comparisons, or a call whose intent is
   not obvious from the expression. Name the variable for what the condition
   *means*, not for what it computes, with an `is` / `has` / `can` / `should`
   prefix, phrased positively.

   ```cpp
   // no
   if (prev.zone == Zone::kPaneEdge && prev.target == leaf && HoldsEdge(dir, u, v)) {

   // yes
   const bool isHoldingPaneEdge =
       prev.zone == Zone::kPaneEdge && prev.target == leaf && HoldsEdge(dir, u, v);
   if (isHoldingPaneEdge) {
   ```

   Leave a **trivial single-term guard inline** — `if (!ptr)`, `if (isOk)`,
   `if (FAILED(hr))`. Naming those adds a line without adding clarity.

   **Preserve behavior exactly.** Place the `const bool` immediately before its
   `if`. Never hoist an `else if` condition above the preceding branch — that
   changes short-circuit evaluation — so an `else if` condition stays inline. The
   same goes for a loop condition that must be re-evaluated each iteration, and
   for a condition whose terms must not all be evaluated (a null check guarding a
   dereference stays in one `&&`, extracted whole).

Rule 1 is enforced by the formatters and linters (clang-format `InsertBraces`
for C++, rustfmt and clippy for the core, oxlint's `curly` for the window). Rule
2 is a review concern, and ESLint's restricted-syntax rule for the window.

# Every language: hardcoded values

- **A closed set of values is an enum** — command codes, modes, kinds, ids,
  message types, zone or direction names, whether they are strings or numbers:
  an `enum class` in C++ and an `enum` in Rust (a TypeScript union when it
  crosses to the window, generated with the protocol). Never the same literal repeated
  at its producer and its consumers.
- **A lone magic number or string is a named constant** (`kName` in C++,
  `SCREAMING_SNAKE_CASE` `const` in Rust, a `camelCase` `const` in TypeScript),
  unless it is self-evident (`0`, `1`, a `/ 2`).
- An Edge ABI value (an RVA, a struct offset, a vtable slot) is not a magic
  number to extract elsewhere: its one home is `src/edge/offsets.h`.

# C++ (`app/native-split/`)

## Naming

| kind | convention | from |
|---|---|---|
| locals, parameters, struct members | `camelCase` | ytm |
| booleans | `is` / `has` / `can` / `should` prefix, positive | ytm |
| functions | `PascalCase` | C++ |
| constants | `kPascalCase` | C++ |
| globals | `g_camelCase` | C++ |
| macros | `SCREAMING_SNAKE_CASE` | C++ |

**Full words, no abbreviations.** `direction`, not `dir`; `webContents`, not
`wc`; `leafRect`, not `lr`. Names of one or two characters fail the linter. The
exceptions are established idioms that would read worse spelled out: `hwnd`,
`hr`, a bare `i` / `j` loop counter, an `x` / `y` coordinate pair, the
`from` / `to` pair, and this codebase's own vocabulary for Edge's central
objects, used throughout `docs/re/`: `mcv` (MultiContentsView), `tsm`
(TabStripModel), `rva`, `dip`.

**Booleans are phrased positively** — `isEnabled`, never `isDisabled` or
`isNotReady`, which force the reader to negate twice at every use.

**Functions stay `PascalCase` and constants `kPascalCase`** — the two places C++
convention wins over ytm's `camelCase` / `SCREAMING_SNAKE_CASE`: in C++
`SCREAMING_SNAKE_CASE` is the macro namespace, and a constant spelled that way
reads as a `#define` and can collide with one from a Windows header.

**A magic number becomes a named `k` constant, and a closed set of them an `enum
class`** (see "hardcoded values" above), unless it is self-evident (`0`,
`1`, a `/ 2`).

Two deliberate exceptions, both because the name is a reference rather than a
choice:

- `src/edge/offsets.h` names every RVA and offset after the reversed symbol —
  `MultiContentsView_SetActiveIndex`, `MCV_containers_data` — so it greps
  against the PDB and `docs/re/`.
- Hook pairs are `h_name` (the detour) and `o_name` (the original it calls).

## Increment and loops

- Post-increment (`i++`) in a `for` clause and as a statement. Where the
  expression's value is read, split the increment into its own statement.
- An infinite loop is `while (true)`, never `for (;;)` (enforced by the linter).

## Structure

- One concern per header, named in its top comment; `edge.h` is the only place
  that calls into `msedge.dll` or reads an Edge struct field, and `offsets.h` is
  the only place an RVA or offset is written.
- Pure logic (no Edge, no Win32) lives in its own header with a unit test under
  `test/` (`pane_tree.h`, `dock.h`, `motion.h`).
- West const: `const Rect rect`, never `Rect const rect`.

## Comments

**Prefer descriptive code over comments** — rename, extract a named `bool`,
constant or function until the code reads like the comment would. Keep a
comment where the code alone cannot be clear: why an offset or byte pattern is
what it is, an Edge quirk being worked around, a required ordering or lifetime,
a dead end recorded so nobody retries it. Delete comments that restate the code,
and above all any comment that no longer matches it.

# The panel (`app/panel-tauri/`)

A Rust core and a WinUI 3 window drawn from TypeScript, ported from the WinUI /
C# panel (in git history before it was removed) and **pixel-identical to it**:
keep it that way. It keeps `%LOCALAPPDATA%\EdgePlus\settings.json` in the shape
every earlier panel used, so existing settings carry over; keep that path and
format as they are. `edge/legacy_install.rs` carries the project's old name
(`FixEdge`) over and holds the old names it has to match (that folder,
`fixedge-split.dll`, `Local\fixedge-split-unload`); the only other old names
left are what `legacy_cleanup.rs` and `startup_entry.rs` match from the
extension and earlier panels. A value the panel and the DLL both need (event
names, the config wire layout, data-folder paths) has one home per side and
must match the DLL's (`app/native-split/src/{config.h,dllmain.cpp,log.h}`).

- **The core** (`src-tauri/src/`, Rust): process, single instance, settings,
  Edge watching, themes, injection and updates, one module per concern —
  `platform/` (paths, settings, the actions the window calls), `dll/` (the
  config wire, its events, the embedded DLL, `dll_sync`), `edge/` (the pinned
  build, the Edge watcher, the dev-mode waiter, legacy carry-over), `themes/`,
  `setup/` (start at sign-in, updates, the MSI hand-off) and `ui/` (the View the
  window renders, every sentence in `wording.rs`, the window's process).
- **`edge-native`** (`src-tauri/edge-native/`) is the **only** crate with
  `unsafe` and every Win32 / WinRT call the core makes; the core forbids
  `unsafe`.
- **The window** (`ui/`, TypeScript) is a separate Node process the core
  starts. It draws WinUI's own controls through dynwinrt's bindings (`pnpm
  restore` generates them into `.winapp/bindings`; `ui/window/winui.ts` is their
  one import site), renders the View the core sends and answers with Actions,
  one JSON line each way. `ui/protocol.g.ts` is generated from
  `src-tauri/src/ui/protocol.rs` (`cargo test export_the_window_types`; the lint
  gate fails when it is stale). WinUI's loop runs on a worker thread. No UI
  library; styles are `ui/window/style/styles.ts`, motion goes through
  `ui/window/style/motion.ts`, which follows Windows' Animation effects switch.
  The Svelte-on-WinUI machinery (the custom renderer, the Vite plugin and hot
  reload, the WinUI start-up) is the generic package `packages/svelte-winui`
  (`DESIGN.md`, `API.md`), a pnpm workspace member with the panel: the panel
  registers its bindings and XAML styles with it. `pnpm lint:package` and
  `pnpm test:js` cover it too.
- **The window kit**: a release exe embeds Node, the bundled scripts, dynwinrt
  (with `vcruntime140.dll` beside its native module, which a clean Windows
  lacks), the Windows App SDK bootstrapper and runtime packages (`pnpm
  build:window`), unpacked under the data folder once per kit; the core
  installs the runtime for the user when it is missing. An exe built inside the
  checkout runs `ui/` with the Node on PATH instead.
- **The embedded split DLL is rebuilt whenever its sources are newer**
  (`build.rs`): a stale one, carried silently, once replaced Edge's current DLL
  at panel start.
- **Shared values** come from the DLL's sources: `// @share Name` markers in
  `app/native-split/src` become `src-tauri/src/platform/shared.g.rs`
  (`tools/Export-SharedConstants.ps1`; the lint gate checks it). Put a marker on
  its own line above the declaration it exports.
- **Naming**: Rust's own (`snake_case` functions and locals, `PascalCase`
  types, `SCREAMING_SNAKE_CASE` constants), `camelCase` in TypeScript;
  `is`/`has`/`can`/`should` booleans and full words everywhere (clippy's
  `min-ident-chars` and oxlint's `id-length` enforce it). The `if` and enum
  rules apply.
- **Build, test, run** (in `app/panel-tauri`): `pnpm restore` once, `pnpm
  build` (the kit, then the release exe), `pnpm test` (`node --test` and `cargo
  test`), `pnpm app` (the full panel from source), `pnpm window` (the window
  alone on `ui/dev/fixture-view.json`, with no core: it never touches Edge, so
  it is the way to work on the UI).
- **Never run the full panel against Avi's Edge** unless asked: at start it
  injects the DLL into every Edge browser process on the pinned build,
  replacing a loaded DLL that differs from the one it carries. To compare it
  with the C# panel, run both in Windows Sandbox (Smart App Control blocks
  unsigned exes there until it is turned off inside the Sandbox) and capture
  each window with `PrintWindow`.

# Linting

- The C++ DLL and the panel: `pwsh tools/lint.ps1` (`-Fix` applies
  clang-format, clang-tidy's fixes and the panel's fixers). The gate is
  clang-format (`.clang-format`), clang-tidy (`.clang-tidy`, correctness as
  errors), a restricted-syntax pass, the `shared.g.rs` freshness check, the
  panel's `pnpm lint:verify` (oxlint, ESLint, clippy at `-D warnings`, rustfmt,
  `tsc` and the fallow audit) and the window protocol's freshness.
  `pwsh tools/lint.ps1 -InstallHook` gates every commit on its staged C++.

# Native split: build and test

- Build: `app/native-split/build.cmd`. Unit tests: `app/native-split/test/run.cmd`
  (every `*_test.cpp`).
- Test only against a throwaway Edge launched with its own `--user-data-dir`
  marker, and select it **only by that marker** — never by process name, start
  time or "the oldest msedge" (Avi's real Edge is running). Kill it the same way.
- Iterate by hot-reload — signal `Local\edgeplus-split-unload`, rebuild, re-inject
  into the same pid — not by relaunching Edge.
- Inject timestamped copies (`edgeplus-split-<ticks>.dll`, a loaded DLL can't be
  overwritten) from `out/`, never from temp/scratchpad. Each new hash triggers a
  Defender cloud scan; `out/` is the one folder Avi excluded from Defender.
- Capture the test window with `PrintWindow`, never the screen.
- The DLL's control channel (`edgeplus.addpane`, codes in `src/split/control.h`) drives
  every pane action from a script, so changes can be verified without clicks.

# Commits

Every commit is one self-contained logical change with a message saying what and
why; land large work as a sequence of them.

# Releases

**Every GitHub release carries both the portable `EdgePlus-<version>.exe` and
the installable `EdgePlus-<version>-setup.msi`**, the MSI built from that same
exe, as `avi/pade` does. Cut one only when asked, with
`pwsh tools\Release.ps1 -Version <x.y.z> -NotesFile <file>` (`docs/RELEASING.md`):
it bumps the core's `Cargo.toml`, builds the window kit and the exe, builds the MSI
(`tools\Build-Installer.ps1`), commits, tags, pushes, creates the release with
both files and then pushes `docs/updates.json` naming both. The notes file holds
the changelog highlights for users — plain sentences, no file names. Never ship
a release without its MSI: a copy the MSI installed is offered only releases
whose manifest names one. A release made some other way gets its MSI added with
`tools\Build-Installer.ps1 -Exe <its exe>` and `gh release upload`.
