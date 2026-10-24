# Releasing an update

1. Bump `<Version>` in `app/panel-winui/Directory.Build.props`. It is the only
   place the version is written: the exe and the panel's footer both read it
   from there.
2. Commit it as `chore(release): <version>`.
3. Build the exe: `dotnet publish app\panel-winui -c Release`. It is one
   `edgeplus.exe` under `app\panel-winui\bin\Release\...\win-x64\publish\`.
   Copy it as `EdgePlus-<version>.exe`.
4. Create a GitHub release `v<version>` with that exe attached. The release
   notes are the changelog: plain sentences for users, no file names.
5. Write the update manifest from the exe that is attached, and push it:

   ```powershell
   pwsh tools\Write-UpdateManifest.ps1 -Exe EdgePlus-<version>.exe
   git add docs/updates.json
   git commit -m "chore(release): update manifest for <version>"
   git push
   ```

   The script refuses an exe whose version is not the one in
   `Directory.Build.props`. Do this last: installed copies see the new release
   only once the manifest is on `main`, so it never points at an exe that is not
   uploaded yet.

## A release for a new Edge build

When Edge updates, every feature that calls into `msedge.dll` pauses until
`app/native-split/src/offsets.h` is re-pinned to the new build. Ship that
release as soon as it is ready, and say in its notes which Edge build it
supports.

## How installed copies update

Installed copies check for a release the way a browser extension checks its
update URL: they read `docs/updates.json` from `main` (served by
`raw.githubusercontent.com`) at start and every six hours. When it names a newer
version, the exe is downloaded in the background and checked against the
manifest's SHA-256, and the window offers **Restart to update**. Restarting
swaps the exe in place, starts the new one, and the new one swaps its own DLL
into Edge, so Edge itself does not restart.

- Only a published exe updates itself. A build run from a checkout never does.
- A copy that cannot write where it is installed (a protected folder) says so
  and points to the releases page.
- The **Check for updates** switch turns the check off, and with it every
  connection Edge+ makes.
- A copy from before this existed (0.0.1) has to be replaced by hand once.

To try it without a release, point `EDGEPLUS_UPDATE_FEED` at a manifest on this
machine (a `http://localhost` address is accepted) and run a published exe.
