# Privacy policy

Last updated: 2026-10-23

Edge+ ("the app") is open source software by Avi (<https://avi12.com>).

**The app collects no personal data and sends no data anywhere.** It has no
accounts, no analytics, no telemetry and no crash reporting, and its own code
makes no network requests.

## What it stores on your device

Only in `%LOCALAPPDATA%\FixEdge`:

- `settings.json`: which features you turned on, and which themes you chose for
  Windows light and dark mode.
- `fixedge-config.bin` and the app's own DLL: the same choices, in the form the
  DLL reads.
- `themes\`: the theme files you added, as you added them.
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

## Children

The app does not collect anything from anyone, including children.

## Changes and contact

If this policy changes, the new text and date are published here, in the
source repository: <https://github.com/avi12/edge-plus>. Questions:
<https://github.com/avi12/edge-plus/issues>.
