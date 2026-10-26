# Releasing an update

Edge+ ships two ways, both attached to every GitHub release:

- **a portable exe**, `EdgePlus-<version>.exe`, that runs from wherever it is
  saved;
- **an MSI installer**, `EdgePlus-<version>-setup.msi`, that installs the same
  exe under `Program Files\Edge+` (`app/installer/`). Its Shortcuts page has two
  checkboxes, both on by default: a desktop shortcut and a Start menu shortcut.
  A silent install turns one off with `DESKTOPSHORTCUT=""` or
  `STARTMENUSHORTCUT=""`.

Either way Edge+ keeps its settings and its DLL in `%LOCALAPPDATA%\EdgePlus`, so
moving between the two keeps everything.

## Cutting a release

With the work committed:

1. Write the notes to a file outside the repository: the changelog
   **highlights**, in plain sentences for users, no file names.
2. Run

   ```powershell
   pwsh tools\Release.ps1 -Version <major.minor.patch> -NotesFile <notes-file>
   ```

`tools/Release.ps1` then:

1. sets the version in `app/panel-tauri/src-tauri/Cargo.toml`, the only place
   it is written: the exe, the MSI and the panel's footer read it from there;
2. builds the window kit and the release exe that embeds it (`pnpm build` in
   `app/panel-tauri`, after `pnpm install` and `pnpm restore`) and copies it to
   `out\release\EdgePlus-<version>.exe`;
3. builds the MSI from that exe (`tools\Build-Installer.ps1`, WiX 5 through its
   NuGet SDK, so nothing to install), so the MSI carries the same bytes. WiX's
   ICE checks are skipped: they run on VBScript, which Windows no longer
   installs by default;
4. commits `chore(release): <version>`, tags `v<version>` and pushes both;
5. creates the GitHub release with the exe and the MSI, appending a link to
   every commit since the previous tag;
6. writes `docs/updates.json` (`tools\Write-UpdateManifest.ps1`) from both
   files' SHA-256 (the exe at the top level, the MSI under `installer`) and
   pushes it.

The manifest goes last: installed copies see a release only once it is on
`main`, so it never points at a file that is not uploaded yet.

A patch release (`0.0.x`) is the default; a minor bump marks a release with a
new feature users would notice.

## A release for a new Edge build

When Edge updates, every feature that calls into `msedge.dll` pauses until
`app/native-split/src/edge/offsets.h` is re-pinned to the new build. Ship that
release as soon as it is ready, and say in its notes which Edge build it
supports.

## How installed copies update

Installed copies check for a release the way a browser extension checks its
update URL: they read `docs/updates.json` from `main` (served by
`raw.githubusercontent.com`) at start and every six hours. When it names a newer
version, the file is downloaded in the background and checked against the
manifest's SHA-256, and the window offers **Restart to update**.

- **A portable copy** downloads the exe. Restarting swaps it in place, starts
  the new one, and the new one swaps its own DLL into Edge, so Edge itself does
  not restart.
- **A copy the MSI installed** (the folder it records under
  `HKLM\Software\EdgePlus`) cannot swap its own exe under Program Files, so it
  downloads the release's MSI instead. Restarting closes Edge+, and a hidden
  PowerShell waits for it to exit, runs the MSI with `msiexec /passive` (Windows
  asks to allow it) and starts Edge+ again. It passes along which shortcuts
  exist, so an update never brings back one the user removed. A release without
  an MSI is not offered to such a copy.
- Only a published exe updates itself. A build run from a checkout never does.
- A portable copy that cannot write where it is saved (a protected folder) says
  so and points to the releases page.
- The **Check for updates** switch turns the check off, and with it every
  connection Edge+ makes.
- A copy from before this existed (0.0.1) has to be replaced by hand once.

To try it without a release, point `EDGEPLUS_UPDATE_FEED` at a manifest on this
machine (a `http://localhost` address is accepted) and run a published exe.
