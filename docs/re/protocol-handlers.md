# Web protocol handlers (`mailto:` -> Gmail)

Target: `msedge.dll` 155.0.4283.45. All addresses are **RVAs**. This is the
research behind "make Gmail open `mailto:` links from Edge". The panel ships
option (b) of section 4 (`app/panel/src-tauri/src/edge_prefs.rs`); nothing here
is used by the DLL.

**Short answer.** Edge ships Chromium's `custom_handlers` stack unchanged and
it works: `navigator.registerProtocolHandler('mailto', ...)` shows an Edge
permission prompt ("wants to Open email links"), and once allowed, a clicked
`mailto:` link navigates to the web handler. On Avi's profile Gmail is already
in the **ignored** list (an old "Block"), and Edge silently drops every later
registration request from an ignored handler, so the prompt never reappears.

Evidence levels: **[run]** observed in a throwaway Edge (own
`--user-data-dir`, killed by that marker only); **[disasm]** read from the
binary; **[infer]** Chromium behaviour not re-checked in this build.

## 1. The registry is present and wired

| piece | rva |
|---|---|
| `ProtocolHandlerRegistryFactory::GetForBrowserContext` | `0x29da376` |
| `ProtocolHandlerRegistryFactory::BuildServiceInstanceForBrowserContext` | `0x2f538b0` |
| `custom_handlers::ProtocolHandlerRegistry::Create` | `0x2f53978` |
| `::InitProtocolSettings` (loads the four prefs) | `0x2f53af8` |
| `::RegisterProtocolHandlersFromPref` / `IgnoreProtocolHandlersFromPref` | `0x2f53caa` / `0x2f53c16` |
| `::SilentlyHandleRegisterHandlerRequest` | `0xbdaee12` |
| `::OnAcceptRegisterProtocolHandler` | `0xbdaf2fe` |
| `::OnIgnoreRegisterProtocolHandler` | `0xbdafa1a` |
| `::RemoveIgnoredHandler` | `0xbdb054c` |
| `::Translate` (mailto: -> handler URL) | `0xbdb0a46` |
| `RegisterProtocolHandlerPermissionRequest::PermissionDecided` | `0xbdadfd0` |
| `ChromeProtocolHandlerRegistryDelegate` (vftable `0x11168ee0`) | `0x2f53950` |
| `ContentSettingRPHImageModel::UpdateAndGetVisibility` (omnibox icon) | `0x27b0840` |
| `ContentSettingRPHBubbleModel` (icon bubble) | `0xa2089d2` |
| `settings::ProtocolHandlersHandler` (settings page back end) | `0xa31c06e` |

`ChromeProtocolHandlerRegistryDelegate` vtable: `[1]` RegisterExternalHandler
`0x8b139f0`, `[2]` IsExternalHandlerRegistered `0x8b13a30`, `[3]`
RegisterWithOSAsDefaultClient `0x8b13a90`, `[4]` CheckDefaultClientWithOS
`0x8b13c90`, `[5]` ShouldRemoveHandlersNotInOS `0x8b13db0` (returns
`GetDefaultSchemeClientSetPermission() != 0`; **not traced** to its caller).

### What `registerProtocolHandler` does [disasm + run]

```
blink NavigatorContentUtils.registerProtocolHandler
 -> LocalFrameHost::RegisterProtocolHandler (mojo)
 -> RenderFrameHostImpl::RegisterProtocolHandler   0x78dbee0
 -> WebContentsImpl::RegisterProtocolHandler       0x79d7780
 -> BrowserWebContentsDelegate::RegisterProtocolHandler  0x9fb6f10
```

The Edge split-view delegates (`SplitWindowWebContentsDelegate`
`0x9ab2ba0`, `SplitWindowRoutedWebContentsDelegate` `0x9ab05a0`) forward to it.
`BrowserWebContentsDelegate::RegisterProtocolHandler`:

1. `SilentlyHandleRegisterHandlerRequest` -> if true, return (no UI).
2. else set the omnibox handler icon's pending handler
   (`PageSpecificContentSettingsDelegate`, +0x28 / +0x108).
3. if the registry's mode (`registry+0x138`) is 1 (CDP `autoAccept`),
   `OnAcceptRegisterProtocolHandler`; otherwise
   `PermissionRequestManager::AddRequest(RegisterProtocolHandlerPermissionRequest)`
   (`0x9fb7181`).

`SilentlyHandleRegisterHandlerRequest` is stock: return true (swallow) when the
scheme is empty, `CanSchemeBeOverridden` fails, the registry is disabled
(`+0xf8`), the handler `IsRegistered`, or it equals an entry of the **ignored**
list (loop over `registry+0x28`, stride `0xe0`, `ProtocolHandler::operator==`);
else `AttemptReplace`.

`PermissionDecided` (`0xbdadfd0`): result 0 (Allow) ->
`OnAcceptRegisterProtocolHandler`; anything else -> `OnIgnoreRegisterProtocolHandler`
(writes the ignored list). So Edge's **Block** is persistent.

### Edge's UI for it [run]

Edge draws the prompt as its own WebUI page `edge://permission-request-dialog/`
(not Chromium's bubble). For a registering origin it reads:
"`<origin>` wants to **Open email links**" with **Block / Just this time /
Allow**. "Allow" -> registered + default; clicking a `mailto:` link then
navigated the tab to `http://localhost:PORT/mail?u=mailto%3A...` (the handler
URL with `%s` filled, URL-escaped). The prompt appeared only when
`registerProtocolHandler` ran with a user gesture (CDP
`Runtime.evaluate(userGesture:true)`); without it, no dialog.

No feature flag gates this: no `kProtocolHandler*` / `RegisterProtocolHandler`
feature symbol exists. Only unrelated flags mention protocol handlers
(`kStripCredentialsForExternalProtocolHandler`,
`kSafelistPaytoToRegisterProtocolHandler`). Policy keys exist
(`custom_handlers.policy`, `protocol_handler.policy` strings).
Settings page `edge://settings/content/handlers` is backed by
`ProtocolHandlersHandler` (HandleSetHandlersEnabled `0xa31c860`, HandleRemoveHandler
`0xa31c7b0`, HandleSetDefault `0xa31c8c0`, GetIgnoredHandlers `0xa31d870`)
**[infer]** (page not opened).

## 2. Where handlers are stored

`Default\Preferences` (plain JSON, **not** in `Secure Preferences`). Pref names
registered in `RegisterProfilePrefs` (`0x297dc06`): `custom_handlers.enabled`
(bool), `.registered_protocol_handlers`, `.ignored_protocol_handlers`,
`.policy.registered_protocol_handlers`, `.policy.ignored_protocol_handlers`
(lists). No separate `default_handlers` key: the default is the entry with
`"default": true`.

Format written by a real Allow [run]:

```json
"custom_handlers": {
  "enabled": true,
  "ignored_protocol_handlers": [],
  "registered_protocol_handlers": [
    { "default": true, "is_allowed_in_incognito": false, "is_confirmed": true,
      "last_modified": "13436736371278250", "protocol": "mailto",
      "security_level": 0, "url": "http://localhost:8768/mail?u=%s" }
  ]
}
```

`last_modified` is Windows FILETIME / 10 (microseconds since 1601), as a string.
Ignored entries have the same shape minus `default` and
`is_allowed_in_incognito`.

Avi's real profile (read-only) today:

- `registered_protocol_handlers`: `[]`
- `ignored_protocol_handlers`: `mailto` -> `https://mail.google.com/mail/?extsrc=mailto&url=%s`
  (2026-04-10) and `webcal` -> `https://calendar.google.com/calendar/u/0/r?cid=%s` (2026-07-06)
- `protocol_handler.allowed_origin_protocol_pairs` (the separate "always allow
  site X to launch OS app Y" list): jetbrains, unityhub, whatsapp, oculus,
  steam, claude, magnet, zoommtg, nxm. Unrelated to web handlers.

**Edge does not re-validate hand-written entries** [run]. A fresh profile whose
`Preferences` held only the `custom_handlers` block above (Gmail URL, no
MACs, no `Secure Preferences`) loaded it, a clicked `mailto:` went to
`mail.google.com` (-> Google sign-in with `continue=...extsrc=mailto&url=mailto:...`),
and the entry was still there after the run. `custom_handlers` is not an
HMAC-tracked pref. Caveats: Edge rewrites `Preferences` from memory on exit
and every ~10 s, so edit only while that profile's Edge is closed; the
OS-default check (`ShouldRemoveHandlersNotInOS`) did not remove it on Windows
in this test.

## 3. Web handler vs OS default for a clicked `mailto:`

Order observed [run] with a registered default: the navigation's
URL-loader throttle (`ProtocolHandlerThrottle::WillStartRequest` `0x29a211a`,
`TranslateUrl`; `Registry::Translate`) rewrites `mailto:` to the handler URL
before any external launch, so `ExternalProtocolHandler::LaunchUrl`
(`0x93a5ea2`) and its dialog (`RunExternalProtocolDialog` `0xa429c2a`) never
run. With **no** registered handler the link falls through to the external
protocol path and the OS default mail app.

- An **ignored** handler is never used for translation and blocks re-prompts
  (verified: seeded ignored entry, `registerProtocolHandler` -> no dialog target
  appeared) [run].
- `ChromeProtocolHandlerNavigationThrottle` (`0x8b13e54`,
  `RunConfirmProtocolHandlerDialog` `0x8b13f80`) is for installed web apps'
  `protocol_handlers`, not for this feature.
- Edge-only extras in the strings: `DefaultEdgeProtocolHandler`,
  `QueryEdgeProtocolHandlerResult` (OS-registration plumbing, not traced).

## 4. Recommendation (best first)

**(a) Documented path, no tooling [run for the mechanism, infer for the UI].**
1. `edge://settings/content/handlers`: delete Gmail from the *not allowed* list
   (this is `RemoveIgnoredHandler` `0xbdb054c`).
2. Open `https://mail.google.com`, and in DevTools run
   `navigator.registerProtocolHandler('mailto','https://mail.google.com/mail/?extsrc=mailto&url=%s')`
   (needs the page focused, i.e. a user gesture; Gmail's own "enable" link does the same).
3. Click **Allow** in the "Open email links" prompt.

The DevTools call is the dependable trigger: it is exactly what Gmail does, and
the prompt flow is verified end to end. Step 1 is required on Avi's profile.

**(b) Write the Preferences while Edge is closed (fits the panel).**
In `edge-native`/panel (Rust, JSON edit of `Default\Preferences`): remove the
Gmail `mailto` entry from `custom_handlers.ignored_protocol_handlers`, append the
object below to `registered_protocol_handlers`, keep `enabled: true`, set
`last_modified` to now (FILETIME / 10). Refuse to write while any msedge
process uses that user-data-dir. Verified accepted and retained [run]; Edge does
not re-validate it.

```json
{"default":true,"is_allowed_in_incognito":false,"is_confirmed":true,
 "last_modified":"<now>","protocol":"mailto","security_level":0,
 "url":"https://mail.google.com/mail/?extsrc=mailto&url=%s"}
```

(For several Google accounts the URL can be `.../mail/u/0/?extsrc=mailto&url=%s`.)
Cost: a restart of Edge, and a race if Edge is open (it overwrites the file).

**(c) Live DLL call [infer, not built].** In the injected DLL, resolve the
profile's registry with `ProtocolHandlerRegistryFactory::GetForBrowserContext`
(`0x29da376`), call `RemoveIgnoredHandler` (`0xbdb054c`) then
`OnAcceptRegisterProtocolHandler` (`0xbdaf2fe`) with a `ProtocolHandler`
(0xe0 bytes; build via `ProtocolHandler::CreateProtocolHandler`-style ctor
`0x2c68d24`). No restart, but it adds three new ABI offsets to `offsets.h` and a
hand-built libc++-string-laden struct; only worth it if the panel must flip it
on a running Edge. A smaller live variant: hook nothing, and just drive (a)
step 2 over a CDP port. Not recommended before (a)/(b).

## Method notes

- Throwaway Edge: `--user-data-dir=<temp> --remote-debugging-port=<p>
  --remote-allow-origins=*`, a localhost page serving a `mailto:` link and a
  `registerProtocolHandler` helper, driven with Node `WebSocket` CDP; killed by
  `--user-data-dir` marker.
- The permission dialog is a separate CDP page target
  (`edge://permission-request-dialog/`); its buttons are Fluent web components
  in shadow DOM.
- Chromium's own implementation lives in `components/custom_handlers/`
  (`protocol_handler_registry.cc`); Edge's matches it function for function, so
  reverse-engineering Chrome was not needed.
