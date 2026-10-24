# Privacy policy

Last updated: 2026-10-24

Edge+ ("the app") is open source software by Avi (<https://avi12.com>).

**The app collects no personal data and sends no data anywhere.** It has no
accounts, no analytics, no telemetry and no crash reporting. The only network
requests its own code makes are to check for a newer version (see *Checking for
updates*), and you can turn that off.

## What it stores on your device

Only in `%LOCALAPPDATA%\EdgePlus`:

- `settings.json`: which features you turned on, and which themes you chose for
  Windows light and dark mode.
- `edgeplus-config.bin` and the app's own DLL: the same choices, in the form the
  DLL reads.
- `themes\`: the theme files you added, as you added them.
- `update\`: a newer version of the app, downloaded and waiting for you to
  restart into it.
- `panel.log` and `split-native.log`: a log of what the app did, kept on your
  device for troubleshooting. They never leave it.

## What it touches in Microsoft Edge

The app loads its own library into Edge's browser process and changes how Edge
draws and lays out its interface, in memory. It does not read your browsing
history, page contents, passwords or sign-in data, and it does not copy
anything out of Edge. "Enhanced find" works on the page you are searching, in
Edge, and "Ask Copilot" sends your question and that page's text to Microsoft
Copilot through Edge, under Edge's and Copilot's own terms and settings, not
through this app.

When you add a theme from a Chrome Web Store or Edge Add-ons link, Edge
downloads it, as it does any extension. The app only hands Edge the link.

## Checking for updates

With the "Check for updates" switch on (it is on by default), the app reads a
small file from this repository, <https://github.com/avi12/edge-plus>, when it
starts and every six hours. If it names a newer version, the app downloads that
version's file from this repository's releases. These are ordinary web
requests, so GitHub sees your IP address and a User-Agent naming the app and its
version, under GitHub's own privacy statement. Nothing else about you or your
Edge is sent. With the switch off, the app makes no network requests at all.

## Children

The app does not collect anything from anyone, including children.

## Changes and contact

If this policy changes, the new text and date are published here, in the
source repository: <https://github.com/avi12/edge-plus>. Questions:
<https://github.com/avi12/edge-plus/issues>.
