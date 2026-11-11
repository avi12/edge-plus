# Releasing an update

Edge+ is installed from one file, **`EdgePlus-<version>.msix`**, attached to every
GitHub release beside **`EdgePlus.cer`**. The package gives Edge+ the identity
it needs: a startup task for start at sign-in, and a clean install, update and
uninstall through Windows. It is signed with the `CN=avi12` code-signing
certificate, which Windows does not trust on its own, so a user trusts
`EdgePlus.cer` once (see the README) before the package installs.

Edge+ keeps its settings and its DLL in `%LOCALAPPDATA%\EdgePlus`, at that real
path (the manifest turns the package's write virtualization off, so Edge can
find the DLL there).

## Cutting a release

With the work committed:

1. Write the notes to a file outside the repository: the changelog
   **highlights**, in plain sentences for users, no file names. The script
   adds the line telling users to install with the `.msix`; don't write it.
2. Run

   ```powershell
   pwsh tools\Release.ps1 -Version <major.minor.patch> -NotesFile <notes-file>
   ```

`tools/Release.ps1` then:

1. sets the version in `app/panel/src-tauri/Cargo.toml`, the only place
   it is written: the exe, the package and the panel's footer read it from
   there;
2. builds the window kit and the release exe that embeds it (`pnpm build` in
   `app/panel`, after `pnpm install` and `pnpm restore`) and copies it to
   `outelease\EdgePlus-<version>.exe`;
3. packs that exe as the MSIX (`tools\Build-Msix.ps1`): the exe, the startup
   launcher, logos drawn from `icons\icon.ico` and `app/package/AppxManifest.xml`
   stamped with the version, signed with the `CN=avi12` certificate from the
   current user's store; it also exports the certificate as `EdgePlus.cer`;
4. commits `chore(release): <version>`, tags `v<version>` and pushes both;
5. creates the GitHub release with the MSIX and the certificate, appending a
   link to every commit since the previous tag;
6. writes `docs/updates.json` (`tools\Write-UpdateManifest.ps1`) from the
   package's SHA-256, under its `msix` key, and pushes it.

The manifest goes last: installed copies see a release only once it is on
`main`, so it never points at a file that is not uploaded yet.

A patch release (`0.0.x`) is the default; a minor bump marks a release with a
new feature users would notice.

## A release for a new Edge build

When Edge updates, every feature that calls into `msedge.dll` pauses until
`app/dll/src/edge/offsets.h` is re-pinned to the new build. Ship that
release as soon as it is ready, and say in its notes which Edge build it
supports.

## How installed copies update

Installed copies check for a release the way a browser extension checks its
update URL: they read `docs/updates.json` from `main` (served by
`raw.githubusercontent.com`) at start and every six hours. When it names a newer
version, the package is downloaded in the background and checked against the
manifest's SHA-256, and the window offers **Restart to update**. Restarting
closes Edge+, and a hidden PowerShell waits for it to exit, installs the
package over the old one (`Add-AppxPackage`) and starts Edge+ again. The new
exe swaps its own DLL into Edge, so Edge itself does not restart.

- Only the installed package updates itself. A build run from a checkout, or a
  bare exe, never does.
- The manifest names the package under `msix`, never as a top-level `url`: a
  copy from before packaging (0.1.0 and earlier) reads that as an exe, so it
  finds nothing to install and has to be replaced by installing the package
  once.
- The **Check for updates** switch turns the check off, and with it every
  connection Edge+ makes.

To try it without a release, point `EDGEPLUS_UPDATE_FEED` at a manifest on this
machine (a `http://localhost` address is accepted) and run the installed
package.
