# Releasing an update

1. Bump `<Version>` in `app/panel-winui/Directory.Build.props`. It is the only
   place the version is written: the exe and the panel's footer both read it
   from there.
2. Commit it as `chore(release): <version>`.
3. Build the exe: `dotnet publish app\panel-winui -c Release`. It is one
   `edgeplus.exe` under `app\panel-winui\bin\Release\...\win-x64\publish\`.
4. Create a GitHub release `v<version>` with the exe attached. The release
   notes are the changelog: plain sentences for users, no file names. The
   footer's version links to the releases page.

## A release for a new Edge build

When Edge updates, every feature that calls into `msedge.dll` pauses until
`app/native-split/src/offsets.h` is re-pinned to the new build. Ship that
release as soon as it is ready, and say in its notes which Edge build it
supports.

## How installed copies update

Installed copies do not update themselves. A user installs a newer
`edgeplus.exe` by running it; at start it swaps its own DLL into Edge in place
of an older one.
