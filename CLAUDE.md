# fix-edge

Live feature switches for Microsoft Edge (see `README.md`):

- `app/native-split/` — C++20 DLL injected into Edge's browser process that grows
  Edge's native split view to N panes; pinned to one `msedge.dll` build
  (`src/offsets.h`), reverse-engineered in `docs/re/`
- `app/panel-winui/` — the panel that toggles each feature and injects the DLL:
  WinUI 3 / C#, with every Win32 call isolated in a `Native/` folder

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

Rule 1 is enforced by the formatters (clang-format `InsertBraces` for C++, and
dotnet format with `app/.editorconfig` for the panel's C#). Rule 2 is a review
concern.

# Every language: hardcoded values

- **A closed set of values is an enum** — command codes, modes, kinds, ids,
  message types, zone or direction names, whether they are strings or numbers:
  an `enum class` in C++ and an `enum` in C#. Never the same literal repeated
  at its producer and its consumers.
- **A lone magic number or string is a named constant** (`kName` in C++,
  `PascalCase` `const` in C#), unless it is self-evident (`0`, `1`, a `/ 2`).
- An Edge ABI value (an RVA, a struct offset, a vtable slot) is not a magic
  number to extract elsewhere: its one home is `src/offsets.h`.

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

- `src/offsets.h` names every RVA and offset after the reversed symbol —
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

# The panel (`app/panel-winui/`)

WinUI 3 / C#, modelled on `avi/any-stt`'s app: unpackaged and self-contained,
its own `Program.Main`, Google C# style from `app/.editorconfig`. It keeps
`%LOCALAPPDATA%\FixEdge\settings.json` in the place and shape the earlier Tauri
panel used, so existing settings carry over; keep that path and format as they
are. A value the panel and the DLL both need (event names, the config wire
layout, data-folder paths) has one home per side and must match the DLL's
(`app/native-split/src/{config.h,dllmain.cpp,log.h}`).

- **Host + swappable panel** (see `app/panel-winui/README.md`): `EdgePlus.csproj`
  is a thin host (process, single instance, window, `App.xaml`, `Host/`);
  `Panel/EdgePlus.Panel.csproj` is everything else, loaded into a collectible
  load context and swapped live on rebuild in a dev run. In the panel: no
  compiled XAML, no WinRT subclasses, CsWinRT's AOT optimizer off, JSON through
  a source-generated context, and every thread / host subscription torn down in
  `PanelView.Dispose` — each of those otherwise pins the old panel.
- **Panel layout**: `Platform/` (paths, settings, the actions the window calls),
  `Dll/` (the config wire, its events, the embedded DLL, `DllSync`), `Edge/`
  (the pinned build, the Edge watcher, installed themes, the dev-mode waiter,
  legacy cleanup), `Themes/`, `Setup/` (start at sign-in), `Ui/`, and `Native/`.
  A `Native/` folder (the panel's or the host's) is the **only** place with a
  `[DllImport]` (and no `unsafe` anywhere; `tools/lint.ps1` enforces it).
- **UI**: WinUI's own controls only — no CommunityToolkit, no UI library. Cards
  are built in code (`PanelView`, `FeatureCard`, `ThemingCard`) from App.xaml's
  styles, whose colours are all `ThemeResource`. Every sentence is in
  `Ui/Wording.cs`; glyphs are code points (dotnet format rewrites a `\u` escape
  into an invisible character). Motion goes through `Ui/Motion.cs`, which
  follows Windows' Animation effects switch live.
- **The embedded split DLL is rebuilt whenever its sources are newer**: a stale
  one, carried silently, once replaced Edge's current DLL at panel start.
- **Shared values** come from the DLL's sources: `// @share Name` markers in
  `app/native-split/src` become `Panel/Platform/Shared.g.cs`
  (`tools/Export-SharedConstants.ps1`, run before every build). Put a marker on
  its own line above the declaration it exports.
- **C# naming** (any-stt's): `PascalCase` types, members and constants,
  `camelCase` locals and parameters, `_camelCase` private fields, `s_camelCase`
  private statics, `is`/`has`/`can` booleans. The `if` and enum rules apply.
- **Tests**: `test/` compiles the pure sources (no WinUI, no Win32) into a
  dependency-free console harness: `app\panel-winui\test\run.cmd`.
- **Never run the full panel against Avi's Edge** unless asked: at start (and
  on every live swap) it injects the DLL into every Edge browser process on the
  pinned build, replacing a loaded DLL that differs from the one it carries.

# Linting

- The C++ DLL and the panel: `pwsh tools/lint.ps1` (`-Fix` applies
  clang-format, clang-tidy's fixes and dotnet format). The gate is clang-format
  (`.clang-format`), clang-tidy (`.clang-tidy`, correctness as errors), a
  restricted-syntax pass, the C# house rules, the `Shared.g.cs` freshness check
  and dotnet format.
  `pwsh tools/lint.ps1 -InstallHook` gates every commit on its staged C++.

# Native split: build and test

- Build: `app/native-split/build.cmd`. Unit tests: `app/native-split/test/run.cmd`
  (every `*_test.cpp`).
- Test only against a throwaway Edge launched with its own `--user-data-dir`
  marker, and select it **only by that marker** — never by process name, start
  time or "the oldest msedge" (Avi's real Edge is running). Kill it the same way.
- Iterate by hot-reload — signal `Local\fixedge-split-unload`, rebuild, re-inject
  into the same pid — not by relaunching Edge.
- Inject timestamped copies (`fixedge-split-<ticks>.dll`, a loaded DLL can't be
  overwritten) from `out/`, never from temp/scratchpad. Each new hash triggers a
  Defender cloud scan; `out/` is the one folder Avi excluded from Defender.
- Capture the test window with `PrintWindow`, never the screen.
- The DLL's control channel (`fixedge.addpane`, codes in `src/control.h`) drives
  every pane action from a script, so changes can be verified without clicks.

# Commits

Every commit is one self-contained logical change with a message saying what and
why; land large work as a sequence of them.
