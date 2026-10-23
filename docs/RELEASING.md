# Releasing an update

1. Bump `<Version>` in `app/panel-winui/Directory.Build.props`. It is the only
   place the version is written: the exe, the panel's footer and the MSIX
   manifest all read it from there.
2. Add a `## <version>` section to the top of `CHANGELOG.md`, written for
   users. It becomes the Store's "What's new", and the footer's version links
   to it.
3. Commit both as `chore(release): <version>`.
4. `pwsh tools/Deploy-Store.ps1 -WhatIf` builds the MSIX and leaves a draft in
   Partner Center. Drop `-WhatIf` to send it to certification.

## A release for a new Edge build

When Edge updates, every feature that calls into `msedge.dll` pauses until
`app/native-split/src/offsets.h` is re-pinned to the new build. Ship that
release with `-Mandatory`: installed panels then install it as soon as it is
downloaded, even with the window open, instead of waiting for it to close.

## How installed copies update

- **Store package:** `Panel/Setup/StoreUpdates.cs` asks the Store 3 minutes
  after start, every 6 hours, and at once when a new Edge build pauses
  features. It downloads silently (only with the user's consent to automatic
  updates) and installs once the window is closed, or right away when the
  update is mandatory. The host registers for restart (`Native/AppRestart.cs`),
  so the panel comes back as the windowless listener after the install.
- **Plain exe:** updates by running a newer `edgeplus.exe`. At start it swaps
  its own DLL into Edge in place of an older one.
