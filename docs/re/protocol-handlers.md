# Web protocol handlers (`mailto:` -> Gmail)

Target: `msedge.dll` 155.0.4283.45. All addresses are **RVAs**. The DLL's
"ask to handle links, like Chrome" switch (`app/native-split/src/theme/protocol_handlers.h`)
is built on section 5.

**Short answer.** Edge ships Chromium's `custom_handlers` stack *and* Chrome's
omnibox UI for it. A gesture-less `navigator.registerProtocolHandler` (what
Gmail does on load) shows Chrome's two-diamond handler icon in the address
bar, and its bubble offers Allow / Deny / **Ignore** (preselected). A request
with a user gesture gets Edge's own permission prompt instead. Both UIs
**silently block the site for good on a non-answer**: the bubble commits its
preselected Ignore however it closes (Esc, its X, a click elsewhere), and the
prompt's "Just this time" writes the same ignored list as Block. Edge then
drops every later request from an ignored handler without a trace, so the
icon never comes back - which is why the icon looks like it "never shows" on
Avi's profile, where Gmail `mailto` and Calendar `webcal` are both ignored.
See section 5.

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
| `::GetIgnoredHandlers` (returns a copy of the list) | `0xbdb065a` |
| `ProtocolHandler::ProtocolHandler(const ProtocolHandler&)` (copy) | `0x2c68d24` |
| `ProtocolHandler::~ProtocolHandler` | `0x2d69d28` |
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
2. **No user gesture** (5th argument, `[rsp+0x1c0]`, false; Gmail's case):
   set the omnibox icon's pending handler (`PageSpecificContentSettingsDelegate`
   +0x28) and the previous default (+0x108, from `GetHandlerFor`), call
   `window->GetLocationBar()->UpdateContentSettingsIcons()` (`[this+0x48]`,
   vtable +0x150 then +0x18) and **return** - no permission request at all.
   This is Chrome's `Browser::RegisterProtocolHandler` exactly.
3. **With a gesture**: clear the pending handler (`EmptyProtocolHandler`),
   update the icons, then if the registry's mode (`registry+0x138`) is 1 (CDP
   `autoAccept`) `OnAcceptRegisterProtocolHandler`, otherwise
   `PermissionRequestManager::AddRequest(RegisterProtocolHandlerPermissionRequest)`
   (`0x9fb7181`).

(An earlier reading of this doc had steps 2 and 3 as one path; the gesture
branch at `0x9fb700c` splits them [disasm].)

`SilentlyHandleRegisterHandlerRequest` is stock: return true (swallow) when the
scheme is empty, `CanSchemeBeOverridden` fails, the registry is disabled
(`+0xf8`), the handler `IsRegistered`, or it equals an entry of the **ignored**
list (loop over `registry+0x28`, stride `0xe0`, `ProtocolHandler::operator==`);
else `AttemptReplace`.

`PermissionDecided` (`0xbdadfd0`) reads a decision `{int value @0, bool
has_value @8}`: no value (prompt dismissed) -> nothing; 0 (Allow) ->
`OnAcceptRegisterProtocolHandler`; **anything else** -> `OnIgnoreRegisterProtocolHandler`
(writes the ignored list). Values seen live: Block = 2, **"Just this time" =
1** - so Edge's "Just this time" blocks the site permanently [run, logged by
the DLL's detour]. Chromium's prompt never offers "Just this time" for
handlers; Edge's generic dialog does.

### Edge's UI for it [run]

Edge draws the prompt as its own WebUI page `edge://permission-request-dialog/`
(not Chromium's bubble). For a registering origin it reads:
"`<origin>` wants to **Open email links**" with **Block / Just this time /
Allow**. "Allow" -> registered + default; clicking a `mailto:` link then
navigated the tab to `http://localhost:PORT/mail?u=mailto%3A...` (the handler
URL with `%s` filled, URL-escaped). The prompt appears only when
`registerProtocolHandler` ran with a user gesture (CDP
`Runtime.evaluate(userGesture:true)`); without one, the omnibox icon below
appears instead (the previous run looked only for a dialog target and missed
it).

No feature flag gates this: no `kProtocolHandler*` / `RegisterProtocolHandler`
feature symbol exists. Only unrelated flags mention protocol handlers
(`kStripCredentialsForExternalProtocolHandler`,
`kSafelistPaytoToRegisterProtocolHandler`). Policy keys exist
(`custom_handlers.policy`, `protocol_handler.policy` strings).
The settings back end `ProtocolHandlersHandler` is compiled in
(HandleSetHandlersEnabled `0xa31c860`, HandleRemoveHandler `0xa31c7b0`,
HandleSetDefault `0xa31c8c0`, GetIgnoredHandlers `0xa31d870`), but **Edge 155
has no page that uses it**: `edge://settings/content/handlers` does not exist,
and the only `allPermissions` route with "handlers" in `msedge.dll` and the
`.pak` files is `paymentHandlers`. So a blocked handler cannot be lifted from
Edge's UI at all (section 6).

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

**(a) Documented path, no tooling [run for the mechanism].**
1. Lift the Gmail block. Chromium does this from
   `chrome://settings/content/handlers`, which Edge 155 lacks (section 1); the
   DLL's switch now does it (section 6).
2. Open `https://mail.google.com`, and in DevTools run
   `navigator.registerProtocolHandler('mailto','https://mail.google.com/mail/?extsrc=mailto&url=%s')`
   (needs the page focused, i.e. a user gesture; Gmail's own "enable" link does the same).
3. Click **Allow** in the "Open email links" prompt.

The DevTools call is the dependable trigger: it is exactly what Gmail does, and
the prompt flow is verified end to end. Step 1 is required on Avi's profile.

**(b) Write the Preferences while Edge is closed (fits the panel).**
In the panel (a JSON edit of `Default\Preferences`): remove the
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

**(c) Live DLL call.** The lifting half is built (section 6): the DLL calls
`RemoveIgnoredHandler` on a copy of each ignored entry. Accepting on the
user's behalf (`OnAcceptRegisterProtocolHandler` `0xbdaf2fe`) is not: once the
block is lifted, the site's own next request lights the icon, and the user
answers it.

## 5. The omnibox icon, and how a non-answer becomes a permanent block

### The icon is there [disasm + run]

- `ContentSettingImageModel::GenerateContentSettingImageModels` (`0x2d40aa6`)
  walks a fixed list of image types at `0x12637240`
  (`0,1,2,5,6,7,8,9,16,10,11,12,13,14,18,21,19,23`); type **8** is
  `PROTOCOL_HANDLERS`, and `CreateForContentType` (`0x2d40c04`, jump-table
  index 8) builds `ContentSettingRPHImageModel`. No feature flag guards it.
- `ContentSettingRPHImageModel::UpdateAndGetVisibility` (`0x27b0840`): visible
  while the delegate's pending handler (+0x28) has a protocol (libc++ string
  size byte at +0x3f). Stock Chromium.
- `LocationBarView::UpdateContentSettingsIcons` (`0x26f18f0`) ->
  `RefreshContentSettingViews` (`0x26f1aae`) updates every view, gating only
  media (9) and sensors (16) on Edge's left-hand indicator flags.
- [run] Throwaway Edge, a localhost page calling
  `registerProtocolHandler('mailto', ...)` on load with no gesture: the
  two-diamond icon appears left of the star, and disappears on a page that does
  not register. Clicking it opens "Allow localhost to open all email links?"
  with **Allow / Deny / Ignore**, Ignore preselected, Done + Manage.

### The bubble commits Ignore on any close [disasm + run]

- `ContentSettingRPHBubbleModel` ctor (`0xa2089d2`): radio labels
  `0x9a55/0x9a56/0x9a57` (Allow, Deny, Ignore); default item from the delegate's
  `pending_protocol_handler_setting` (+0x1f0): 1 (allow) -> 0, 2 (block) -> 1,
  else (0, the normal case) -> **2, Ignore**.
- `ContentSettingBubbleContents::WindowClosing` (`0x96f0030`) calls the model's
  vtable +0x48 = `ContentSettingRPHBubbleModel::CommitChanges` (`0xa2090a0`)
  for **every** close of the bubble widget.
- `CommitChanges` -> `PerformActionForSelectedItem` (`0xa2090ec`, reads
  `ContentSettingBubbleContents::GetSelectedRadioOption`): 0 ->
  `RegisterProtocolHandler` (`RemoveIgnoredHandler` + `OnAccept...`), 1 ->
  `UnregisterProtocolHandler` (`OnDeny...`, registered but not default), 2 ->
  `IgnoreProtocolHandler` (`OnIgnoreRegisterProtocolHandler`); then clears the
  pending handler (icon gone) and refreshes the location bar.
- The Done button is the dialog's accept callback: the ctor (`0x96ef1b6`) binds
  the model's `vcall'{56}` (vtable +0x38, `OnDoneButtonClicked`) with
  `SetAcceptCallback`. For the RPH model that slot holds the shared empty
  virtual `0x5466b60`; Done runs it, then the widget closes -> `CommitChanges`.
- [run, stock Edge] open the bubble, press **Esc** -> `ignored_protocol_handlers`
  gains the entry; open it, **click the page** -> same. Ignore is the default
  and the commit is unconditional, so a glance at the bubble blocks the site.
  The same code is in upstream Chromium; Chrome behaves the same way.
- Callers of `OnIgnoreRegisterProtocolHandler` (scan of every `E8/E9 rel32` in
  `.text`): exactly two, `IgnoreProtocolHandler` (`0xa20922e`) and
  `PermissionDecided` (`0xbdae012`). So the two non-answers above are the only
  ways a request lands in the ignored list from the UI.

**So, Avi's ignored Gmail / Calendar entries** [infer, consistent with all of
the above]: Gmail and Calendar register on load without a gesture; the icon
appeared; the bubble was opened once and closed without picking Allow (or
Done pressed on the preselected Ignore). From then on
`SilentlyHandleRegisterHandlerRequest` swallows every request, so the icon never
returns. Nothing auto-dismisses a gesture-less request: it never reaches the
permission manager.

### The fix (`protocol_handlers.h`) [run]

Least invasive, Chrome's UI kept:

1. Point the RPH bubble model's Done slot (`vftable 0x1123cd80 + 0x38`) at our
   `OnDoneButtonClicked`, which records the model; hook `CommitChanges` to run
   Edge's commit **only** for that model. Any other close does nothing, and the
   pending handler stays, so the icon remains for a later answer. Allow + Done,
   Deny + Done and Ignore + Done keep Edge's behaviour.
2. Hook `PermissionDecided`: a decision of 1 ("Just this time") returns without
   calling Edge's, so nothing is remembered. Allow (0) and Block (2) unchanged.

Verified in a throwaway Edge with the DLL injected (config written with only
this switch on): icon shown; Esc and click-away logged "closed without Done"
with no `custom_handlers` written and the icon still shown; Allow + Done wrote
the default handler and a clicked `mailto:` navigated to
`/mail?u=mailto%3Atest%40example.com`; the prompt's "Just this time" wrote
nothing, Block still wrote the ignored entry; with the switch off, Esc wrote
the ignored entry as stock.

This stops new accidental blocks; the old ones are lifted by section 6.

## 6. Lifting the blocks already on record

Edge 155 has no page that lists or removes ignored handlers (section 1), so a
site blocked by an old non-answer stays silent forever. Whenever the switch
turns on - the DLL loading with it on, or the panel switching it on - the DLL
lifts every block in each profile that has a browser window open, and logs
`[handlers] lifted N blocked link handler(s)`. A block made after that (Ignore
+ Done, or the prompt's Block) sticks until the switch next turns on or the
DLL next loads.

### The calls [disasm]

- `ProtocolHandlerRegistryFactory::GetForBrowserContext` (`0x29da376`):
  `GetInstance`, then `GetServiceForBrowserContext(factory, context, create=1)`.
  The profile comes from each browser window's active tab
  (`WebContentsImpl::GetBrowserContext`), as theming does.
- The live ignored list is a size-based libc++ vector at **registry+0x28**:
  `{ProtocolHandler* data @+0x28, size_t size @+0x30, capacity @+0x38}`, element
  stride **0xe0**. `GetIgnoredHandlers` (`0xbdb065a`) copies exactly
  `[+0x28] .. [+0x28] + [+0x30] * 0xe0`; `RemoveIgnoredHandler` and
  `SilentlyHandleRegisterHandlerRequest` walk the same span.
- `RemoveIgnoredHandler(this, const ProtocolHandler& h, bool save)`
  (`0xbdb054c`): returns unless `h` is in the live list (+0x28) **and** the
  user's list (+0x80; "user" and "policy" below follow Chromium's member
  order [infer]); erases it from the user's list; `save` (an Edge
  addition; the bubble's Allow path passes 1, `0xa209162`) calls `Save`
  (writes the prefs); then unless the policy list (+0x98) also holds it,
  erases it from the live list and `NotifyChanged`. Its erases compare
  against `h` while shifting the vector, so `h` must not point into the list.
- `ProtocolHandler` is 0xe0 bytes: protocol `std::string` @0, `GURL` @0x18,
  two `optional<std::string>` @0x90 / @0xb0 (engaged flags @0xa8 / @0xc8), 16
  bytes @0xd0. Its copy constructor is `0x2c68d24` (the one
  `vector<ProtocolHandler>::__init_with_size` calls per element; the other
  ctor, `0xbdb1be8`, takes the fields), its destructor `0x2d69d28`.
- Edge itself lifts every block this way: `ClearUserDefinedHandlers` calls
  `GetIgnoredHandlers` for a copy, then `RemoveIgnoredHandler` per element.

### The DLL (`protocol_handlers.h`, `LiftIgnoredHandlers`)

Per profile, on the UI thread: copy every entry of the live list into storage
the DLL owns with Edge's copy constructor, call `RemoveIgnoredHandler(copy,
save=true)` for each, then Edge's destructor on each copy. The count lifted is
the live list's size before minus after, so a policy block (which stays) is
not counted. A profile with several windows is visited once per window; the
later visits find nothing.

### Verified [run]

Throwaway Edge (own `--user-data-dir`, `LOCALAPPDATA` redirected), its
`Default\Preferences` seeded before launch with one ignored `mailto` handler
for a localhost page that calls `registerProtocolHandler` on load without a
gesture:

1. Without the DLL: the page registered, no icon (the request was swallowed).
2. DLL injected with the switch on: `[handlers] on`, `[handlers] lifted 1
   blocked link handler(s)`.
3. Page reloaded: the handler icon is back left of the star.
4. About 12 s later Edge had rewritten `Preferences`:
   `ignored_protocol_handlers` is `[]`.

Not run live: the switch turning on through the config event (another Edge
had the DLL loaded, and the event would have reached it). It takes the same
`LiftWindowHandlerBlocks` pass as the load, from the same `ApplyConfig`.

## Method notes

- Throwaway Edge: `--user-data-dir=<temp> --remote-debugging-port=<p>
  --remote-allow-origins=*`, a localhost page serving a `mailto:` link and a
  `registerProtocolHandler` helper, driven with Node `WebSocket` CDP; killed by
  `--user-data-dir` marker.
- Testing the DLL next to a real Edge that may have it loaded: launch the
  throwaway Edge with `LOCALAPPDATA` pointed at a scratch folder, so the DLL
  reads its config from (and logs to) there, and write the config before
  injecting. Never signal `Local\edgeplus-config-changed` /
  `Local\edgeplus-split-unload` from a test: they are session-wide, so they
  would reach the real Edge's DLL too. Kill the test Edge by marker instead of
  unloading.
- The bubble is a separate `Chrome_WidgetWin_1` window (344 x 284); posted
  clicks and Esc reach it, and `PrintWindow` captures it.
- The permission dialog is a separate CDP page target
  (`edge://permission-request-dialog/`); its buttons are Fluent web components
  in shadow DOM.
- Chromium's own implementation lives in `components/custom_handlers/`
  (`protocol_handler_registry.cc`); Edge's matches it function for function, so
  reverse-engineering Chrome was not needed.
