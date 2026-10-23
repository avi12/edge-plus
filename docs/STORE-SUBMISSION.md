# Microsoft Store listing

Everything the Partner Center submission asks for, in the order it asks. Each
"paste verbatim" block is one line per paragraph; this file wraps at 80 columns
and a reflowed block would paste hard line breaks into the middle of a sentence.

## Read this first

- **Expect certification to push back.** Edge+ loads a DLL into Edge's browser
  process and hooks `msedge.dll` in memory. A Store scan reads that as tampering
  (policies 10.2 security, 10.2.3, 10.2.8 unsupported methods). The notes below
  disclose it in full up front, which is the only version of this submission
  that can end in a human waiver. Hiding it guarantees rejection.
- **The name.** "Microsoft Edge" is a Microsoft trademark and 10.1 forbids
  implying an affiliation. A name that *starts* with "Edge" is the risky kind;
  the safe pattern is the product's own name plus "for Microsoft Edge" as a
  descriptor. Reserve one of these, in this order of preference:
  1. `Plus for Edge`
  2. `Tab Plus for Microsoft Edge`
  3. `Edge+` (the project's name; keep it for the repo, but expect a rename
     request at certification).
- **A package needs a reserved name first.** `msstore` can edit and submit a
  listing that already exists, but it cannot reserve a name. Reserve it in
  Partner Center (Apps and games > New product > MSIX or PWA app), then copy
  *Product identity* (Package/Identity/Name, Package/Identity/Publisher,
  Package/Properties/PublisherDisplayName) into the manifest. Those three must
  match Partner Center character for character.
- **The MSIX is full trust.** The one capability is `runFullTrust`, which
  needs approval; the justification is the certification notes below.

## Product

| Field | Value |
|---|---|
| Product type | MSIX or PWA app |
| Category | Utilities & tools |
| Price | Free, no add-ons, no in-app purchases |
| Markets | All |
| Age rating (IARC) | Utility, no user-generated content, no online interaction: 3+ |
| Source code | https://github.com/avi12/edge-plus (Apache License 2.0) |
| Support | https://github.com/avi12/edge-plus/issues |
| Privacy policy | https://github.com/avi12/edge-plus/blob/main/docs/PRIVACY.md |
| Website | https://avi12.com |

## System requirements

Minimum (Partner Center asks for these as lists, one line each):

- OS: Windows 10 version 2004 (build 19041) or later, 64-bit. Windows 11
  recommended.
- Browser: Microsoft Edge Stable, 64-bit, exactly version 155.0.4283.62. On any
  other version the app turns the Edge features off and leaves Edge untouched.
- Architecture: x64.
- Memory and disk: no special requirements; about 150 MB of disk (the app is
  self-contained and carries its own .NET and Windows App SDK runtime).
- Account: none. No sign-in, no network access needed.

The manifest's `TargetDeviceFamily` is `Windows.Desktop`, `MinVersion`
`10.0.19041.0`, `MaxVersionTested` `10.0.26100.0`, the same floor as
`TargetPlatformMinVersion` in `EdgePlus.csproj`.

## Description (paste verbatim)

```text
Edge+ adds live switches to Microsoft Edge. Turn a feature on in the Edge+ panel and Edge picks it up at once. No restart, no extension to install.

SPLIT VIEW WITHOUT THE LIMIT
Edge's own split view stops at two panes. Edge+ grows it to as many as you like, side by side or stacked, inside the tab. Every pane has its own address bar, the dividers drag, and a pane's menu adds a pane above, below, left or right. Move between panes with Shift + Alt and an arrow key. Alt-drag a tab onto the split to add it.

THEME BY WINDOWS LIGHT AND DARK
Pick one Edge theme for Windows light mode and another for dark mode. They switch the moment Windows does. Add themes from a Chrome Web Store or Edge Add-ons link, or from a local file or folder. Edge's own theming paints them.

CUT-CORNER TABS
Every tab, in the vertical list and in the horizontal strip, gets a straight leading edge and a 45 degree cut on its trailing corner. The selected tab is filled in your theme's accent color.

MORE
Live tab preview on hover, so a hovered tab plays its page live instead of showing a frozen snapshot. Settings and extensions pages that follow your theme. Enhanced find: type /pattern/ for a RegExp search, or start with ? to ask Copilot and highlight the paragraphs it names. Ask to handle links like Chrome does, so a site such as Gmail can offer to open email links. Hide the developer-mode extensions prompt.

HOW IT WORKS, IN PLAIN WORDS
Edge+ is open source (Apache License 2.0). It loads a small library into Edge's browser process and changes how Edge draws and lays out its own interface, in memory. It never edits Edge's files, collects nothing, and makes no network requests of its own. Turn a switch off and its change is undone. Its settings stay in a folder you can delete (%LOCALAPPDATA%\FixEdge).

IMPORTANT
This version of Edge+ works with one Edge version only: 155.0.4283.62. On any other version it does nothing and leaves Edge alone, and the panel says so. Edge+ is an independent project and is not affiliated with or endorsed by Microsoft.
```

### Short description (100 characters)

```text
Live switches for Microsoft Edge: N-pane split view, light and dark themes, cut-corner tabs.
```

### Features (up to 20 lines, 200 characters each)

```text
Split view with as many panes as you like, side by side or stacked, in one tab
Switch Edge themes live when Windows flips between light and dark
Cut-corner tabs in the vertical list and the horizontal strip
Hover a tab to see its page live instead of a frozen thumbnail
Enhanced find: RegExp search, and ask Copilot to highlight the answer
Settings and extensions pages follow your Edge theme
Ask to handle links like Chrome, so Gmail can open your email links
Every change is live: no restart, no extension, nothing to pre-install
Open source under the Apache License 2.0
```

### Search terms (up to 7, 40 characters each)

```text
edge
split view
vertical tabs
edge theme
dark mode
tab manager
browser tools
```

### What's new in this version

```text
First release.
```

## Notes for certification (paste verbatim)

```text
Edge+ adds features to Microsoft Edge by loading its own DLL (fixedge-split.dll, embedded in the app and unpacked to %LOCALAPPDATA%\FixEdge) into Edge's browser process. Everything privileged it does, up front, because a scanner will read it as tampering:

1. It finds the running msedge.exe browser processes of the SAME user and loads the DLL into them with the standard remote-thread LoadLibrary technique. It does this only when the Edge build is exactly 155.0.4283.62, the one the DLL was written against; on any other build it injects nothing.

2. Inside that process the DLL hooks functions of msedge.dll IN MEMORY (MinHook, open source) to change how Edge draws tabs and lays out its split view. No file of Edge is modified on disk; every change dies with the process.

3. It reads and writes only: its own folder %LOCALAPPDATA%\FixEdge (settings.json, a config file, the DLL); and, for "Hide the developer-mode prompt" and for adding a theme, Edge's own Preferences / external-extension mechanism (a theme is installed the way Edge installs any external extension, from a .crx the user chose).

4. Turning a switch off, or turning Edge+ off, signals the DLL to undo its changes and unload itself from Edge; no restart of Edge is needed. Uninstalling the app leaves only its settings folder, which is plain files you can delete.

No other process is touched, nothing is packed or obfuscated, and the source is public: https://github.com/avi12/edge-plus (Apache License 2.0; the reverse-engineering notes are in docs/re). The app has no network code and sends no data anywhere.

TO TEST: install Microsoft Edge Stable 155.0.4283.62 (the panel shows a banner on any other build and does not inject). Open Edge, then open Edge+. Turn on "Split into any grid", open a tab in Edge, and use a pane's three-dot menu to add a pane: the tab grows a third pane. Turn on "Cut-corner tabs" with Theming on and a theme chosen to see the tab shape change. Turn the switches off and the changes are undone.

The injection is the product's entire function, not a side effect of it. Privacy policy: https://github.com/avi12/edge-plus/blob/main/docs/PRIVACY.md
```

## Alternative if the package is rejected

The Store route that can pass for an app that injects is the **EXE or MSI app**
product type: it lists a URL to a signed installer and takes no price tier (free
is what we want). It needs an installer signed with a certificate that chains to
a trusted root (OV or EV code signing) and silent install and uninstall
switches. Edge+ ships neither yet. The GitHub release of the single-file
`edgeplus.exe` (`dotnet publish app\panel-winui -c Release`) is the version that
does not depend on the Store.

## Submission with the CLI

`msstore` is already configured on this machine (`msstore info`). Once the
product exists in Partner Center and the package is built (see
`tools/Package-Msix.ps1`):

```powershell
msstore apps list                                  # find the product's ID
msstore submission status <productId>
msstore publish <path-to-msix-folder-or-msix> -id <productId> --noCommit
```

`--noCommit` creates the submission as a draft without submitting it; review it
in Partner Center, then commit it there or with `msstore submission publish`.
