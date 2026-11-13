# Releasing an update

Edge+ installs per user from one file, **`EdgePlus-Setup.exe`**, attached to
every GitHub release beside **`EdgePlus-<version>.zip`** and **`releases.json`**.
There is no installer service, administrator prompt or certificate: opening the
setup exe copies it to `%LOCALAPPDATA%\EdgePlus\app-<version>`, puts a stable
`edgeplus-launcher.exe` in the root, adds a Start menu shortcut and an entry in
Add or remove programs, and starts it. The zip and `releases.json` are what
installed copies update from (the format of winui-svelte's `createRelease`).

Edge+ keeps its settings and its DLL in the same root, `%LOCALAPPDATA%\EdgePlus`.

## Cutting a release

With the work committed:

1. Write the notes to a file outside the repository: the changelog
   **highlights**, in plain sentences for users, no file names. The script
   adds the line telling users to install with the setup exe; don't write it.
2. Run

   ```powershell
   pwsh tools\Release.ps1 -Version <major.minor.patch> -NotesFile <notes-file>
   ```

`tools/Release.ps1` then:

1. sets the version in `app/panel/src-tauri/Cargo.toml`, the only place it is
   written: the exe, the zip and the panel's footer read it from there;
2. commits `chore(release): <version>` and pushes `main`;
3. creates the GitHub release `v<version>` with the notes, appending a link to
   every commit since the previous tag. Creating it creates the tag.

The tag starts `.github/workflows/release.yml` on GitHub, which:

1. checks the tag names the version in `Cargo.toml`;
2. builds the window kit and the release exe that embeds it (`Build-ReleaseExe`:
   `pnpm install`, `pnpm restore` and `pnpm build` in `app/panel`, rebuilding the
   embedded split DLL if its sources are newer) and the launcher;
3. packs the release (`pnpm create-release`, `app/panel/scripts/create-release.ts`):
   `edgeplus.exe` and `edgeplus-launcher.exe` as `EdgePlus-<version>.zip`,
   added to `releases.json` with its SHA-256 and size, and `EdgePlus-Setup.exe`;
4. attaches the three files to the release with `gh release upload`.

Installed copies read `releases/latest/download/releases.json`, so they see a
release once the workflow has attached it; until then the feed is the previous
release's.

A patch release (`0.0.x`) is the default; a minor bump marks a release with a
new feature users would notice.

## A release for a new Edge build

When Edge updates, every feature that calls into `msedge.dll` pauses until
`app/dll/src/edge/offsets.h` is re-pinned to the new build. Ship that
release as soon as it is ready, and say in its notes which Edge build it
supports. The DLL stays pinned to one build: an update replaces Edge+ and the
DLL it embeds together, and the new exe swaps the DLL into Edge itself.

## How installed copies update

Installed copies check for a release the way a browser extension checks its
update URL: they read `releases.json` from the latest release at start and every
six hours. When it names a newer version, the zip is downloaded in the
background, checked against the feed's size and SHA-256, unpacked into
`app-<version>.staging` and renamed into place last, and the window offers
**Restart to update**. Restarting closes Edge+ and starts the launcher with
`--apply`: it waits for Edge+ to end, runs the new version's `--updated` hook,
starts the newest `app-<version>` and, if that does not start, marks it bad and
starts the previous one. The new exe refreshes the Add or remove programs entry,
and removes the older versions once it is running. It swaps its own DLL into
Edge, so Edge itself does not restart. The launcher is the Rust crate
`winui-svelte-launcher` (a git dependency of the core and of
`app/panel/src-tauri/launcher`), pinned by `rev` in both `Cargo.toml` files.

- Only an installed copy updates itself. A build run from a checkout never does.
- The **Check for updates** switch turns the check off, and with it every
  connection Edge+ makes.

To try it without a release, point `EDGEPLUS_UPDATE_FEED` at a `releases.json`
on this machine (a `http://localhost` address is accepted, with the zip beside
it) and run the installed copy.

## Uninstalling

Add or remove programs runs `edgeplus-launcher.exe --uninstall`, which starts the
newest exe's uninstall: it removes the start-at-sign-in entry, the programs
entry, the Start menu shortcut and the toast registration. A hidden PowerShell
then ends the processes running from the root (Edge+'s own, never Edge's),
signals the DLL to unload itself from Edge, which keeps running, and deletes the
root, settings included.
