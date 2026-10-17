# Gemini in Chrome (`glic`) and Edge's Copilot button

Targets: Chromium source at tag `155.0.8059.85` (the Chrome build installed
beside Edge), `chrome.dll` 155.0.8059.85, and `msedge.dll` 155.0.4283.45. All
Edge addresses are **RVAs**; any the DLL uses live only in
`app/native-split/src/offsets.h`.

The question: can Edge's chat button open Gemini, with the page context and
multi-tab sharing Gemini gets in Chrome?

**Short answer.**

- Chrome's Gemini is the `glic` feature: a per-profile service, a toolbar
  button, a side panel or floating window holding the WebUI `chrome://glic`,
  which hosts `https://gemini.google.com/glic` in a `<webview>` and gives it a
  large JS API (`GlicBrowserHost`) for tab context, pinned tabs, the
  microphone, the actor (agentic browsing) and more.
- **Edge does not ship the glic host.** `msedge.dll` has none of
  `GlicKeyedService`, `GlicButton*`, `GlicWindowController`, `GlicUI`,
  `glic::Host`, the sharing managers or `glic::mojom::*` stubs. Only shared
  leftovers ship (the actor framework, contextual cueing, feature flags, the
  page-content fetcher). There is no dormant glic to switch on.
- **The Gemini web client does not need Chrome to boot, only a host.** It
  waits for one `postMessage` from origin `chrome://glic` carrying the API
  library as source, and it ships its own fake host for development
  (`?glic=fake`). With the fake host, the page boots in Edge and stops at
  "Gemini isn't available right now" while signed out. So a host that speaks
  the same JS API can be supplied from outside Chrome.
- Edge's chat button is `EdgeCopilotToolbarLabelButton`, driven by an
  "action" object, `EdgeDiscoverToolbarButtonActionEdgeCopilot`, that supplies
  its image, tooltip, accessible name and click. Hooking those five functions
  reskins the button and reroutes it without touching the button class.

Evidence levels: **[src]** read in the Chromium source at the tag above;
**[run]** observed in a throwaway Edge (own `--user-data-dir` and
`LOCALAPPDATA`, killed by that marker only); **[disasm]** read from the binary
or its public PDB; **[infer]** reasoned, not checked.

## 1. How glic is enabled and gated [src]

Everything lives in `chrome/browser/glic/`. The `public/` folder holds the
entry points (`glic_enabling.cc`, `glic_keyed_service.cc`, `features.cc`).

| gate | where | rule |
|---|---|---|
| build | `chrome/browser/glic/BUILD.gn` | the browser half is its own target set; Edge's build leaves it out (section 4) |
| kill switch | `features::kGlic` (`chrome_features.cc`) | on by default on Windows, macOS, ChromeOS and Android |
| rollout | `features::kGlicRollout`, or the tiered rollout (`kGlicTieredRolloutV2`) | either must pass, unless `--glic-dev` |
| country | `kGlicCountryFiltering` (on), params `enabled_countries` / `disabled_countries` | the variations "permanent" and "session" country must be listed; the built-in default list is about 150 countries in four phases, starting `us,ca,nz,in` |
| locale | `kGlicLocaleFiltering` (on), params `enabled_locales` / `disabled_locales` | same scheme for the UI locale |
| profile | `ComputeProfileEnablement` | a regular profile (no guest, incognito or system profile) |
| account | same | a signed-in primary account whose capabilities allow adult features or `can_use_gemini_in_chrome`; a refresh token in an error state marks it "not fully signed in" |
| policy | `optimization_guide::prefs::kGeminiSettings` | the enterprise `GeminiSettings` policy must be "enabled" |
| server | `kGlicUserStatusCheck`, `https://geminiweb-pa.googleapis.com/v1/glicStatus` | a cached `DISABLED_BY_ADMIN` / `DISABLED_OTHER` verdict disables it |
| onboarding | `glic::prefs::kGlicCompletedFre` | the first-run (FRE) consent; until then the button opens the FRE |
| system | `GlicGlobalEnabling` | minimum RAM (`glic-min-required-ram-mb`) and OS version |

`--glic-dev` skips the rollout and account checks. `--glic-guest-url` (or the
Finch param `glic-guest-url` of `kGlicURLConfig`) replaces the guest URL,
which defaults to `https://gemini.google.com/glic`; `GetLocalizedGuestURL`
appends `hl=<Google locale>`. `kGlicGuestUrlPresets` can point it at the
autopush, staging or preprod servers instead. The same strings are in the
shipped `chrome.dll` (`https://gemini.google.com/glic` at file offset
`0x10916b80`, `chrome://glic/*`, `glic-guest-url`, `glic-dev`,
`GlicKeyedService`). [run]

## 2. The button, the panel and the client [src]

- **Button.** `GlicButtonController` (`browser_ui/glic_button_controller.cc`)
  shows or hides the button per profile from
  `GlicEnabling::ShouldShowGlicButton` and the pref `kGlicPinnedToTabstrip`. It
  drives a split button through `GlicSplitButtonDelegate` in the horizontal
  tab strip and in vertical tabs; `kGlicToolbarButtonLocation` can move it into
  the toolbar. Its element id is `kGlicButtonElementIdProvider`, which is even
  present in `msedge.dll` (`0x12d75180`) because the id table is shared.
  [disasm]
- **Other entry points.** A keyboard shortcut (`glic_hotkey.cc`), an optional
  OS-wide hotkey and a system-tray icon (`kGlicChromeStatusIcon`), the
  context menu ("Ask Gemini", `kGlicContextMenu`), contextual cue chips in
  the address bar (`suggestions/`), and the PDF viewer's "summarize" button.
- **Panel.** Since `kGlicMultiInstance` (on by default) each tab can hold its
  own Gemini instance in a side panel entry `SidePanelEntry::Id::kGlic`
  (`widget/glic_side_panel_coordinator_impl.cc`, minimum width 384). An
  instance can also be detached into a floating widget (`GlicFloatingUi`,
  `GlicWidget`), the older single-window design.
- **Client.** The panel holds the WebUI `chrome://glic`
  (`host/glic_ui.cc`, `resources/glic/`). Its page creates a `<webview>` with
  partition `persist:glicpart` and loads the guest URL. Every guest request
  carries `X-Glic: 1`, `X-Glic-Chrome-Version` and `X-Glic-Chrome-Channel`
  (`shared/glic_request_headers.ts`). `GlicCookieSynchronizer` copies the
  Google sign-in cookies into that partition.

## 3. The host <-> web client API [src] [run]

### Boot

1. The WebUI page posts `{type: 'glic-bootstrap', glicApiSource}` into the
   webview (`webview.ts`). `glicApiSource` is the API client library as text.
2. The guest's listener accepts it only when `event.origin === 'chrome://glic'`
   and `event.source !== null`. It runs the source as a `<script>` through a
   Trusted Types wrapper, then calls `window.internalAutoGlicBoot(source)`,
   which returns a `GlicHostRegistry`. [run: the shipped Gemini bundle does
   exactly this, function `rle` in its main module]
3. The client calls `registry.registerWebClient(client)`; the host calls
   `client.initialize(browserHost)` and later
   `client.notifyPanelWillOpen({kind: ATTACHED, ...})`.
4. Underneath, the library speaks a request/response protocol over
   `postMessage` to the WebUI (`glic_api_impl/transport/`), which forwards
   each request to the browser over the mojo interface `glic.mojom.WebClientHandler`
   (`host/glic.mojom`); `glic.mojom.WebClient` carries notifications back.

The Gemini bundle also contains a complete **fake host**, selected by
`?glic=fake` (`GlicHostRegistryFactory` picks `new Xle` instead of `rle()`).
Loaded at `https://gemini.google.com/glic?glic=fake&hl=en` in a throwaway
Edge, the page boots: the tab title becomes the fake tab's ("Gemini Chrome ::
New Conversation"), the panel controls appear, and the body reads "Something
went wrong / Gemini isn't available right now": the server refuses the
signed-out user. Without `?glic=fake` the page waits for the boot message and
shows the same error. [run]

### The `GlicBrowserHost` surface

The fake host lists what the shipped client calls (all optional in
`glic_api.ts`; an `Observable` is `{subscribe(cb), subscribeObserver?(o)}`
returning `{unsubscribe()}`, an `ObservableValue` adds `getCurrentValue()`):

| area | methods |
|---|---|
| identity | `getChromeVersion`, `getPlatform`, `getFormFactor`, `getHostCapabilities`, `getUserProfileInfo`, `getModelQualityClientId`, `getGeminiEnterpriseSettings`, `refreshSignInCookies`, `maybeRefreshUserStatus` |
| panel | `getPanelState` (`HIDDEN` 0, `DETACHED` 1, `ATTACHED` 2), `panelActive`, `canAttachPanel`, `isBrowserOpen`, `attachPanel`, `detachPanel`, `closePanel`, `resizeWindow`, `setMinimumWidgetSize`, `enableDragResize`, `isManuallyResizing`, `getZoomLevel` |
| focused tab | `getFocusedTabStateV2` -> `{hasFocus: {tabData}}` or `{hasNoFocus: {tabFocusCandidateData?, noFocusReason}}`; `getContextFromFocusedTab(options)` |
| other tabs | `getPinCandidates({maxCandidates, query?})`, `pinTabs(ids)`, `unpinTabs(ids)`, `unpinAllTabs`, `getPinnedTabs`, `setMaximumNumberOfPinnedTabs`, `getContextFromTab(id, options)`, `getTabById`, `getTabFaviconById`, `activateTab`, `activateTabWithUrl`, `createTab(url, options)`, `openPinnedTabPicker` |
| page context | `TabContextOptions` = `innerText`, `innerTextBytesLimit`, `viewportScreenshot`, `annotatedPageContent` (+ mode), `maxMetaTags`, `pdfData`, `pdfSizeLimit`; the result is `{tabData, webPageData: {mainDocument: {origin, innerText, innerTextTruncated}}, viewportScreenshot, pdfDocumentData, annotatedPageData}`. Also `getPageMetadata`, `getImageBytesFromTab`, `captureRegion`, `scrollTo` / `dropScrollToHighlight` (citations) |
| permissions | microphone, location and tab-context permission getters and setters, `getOsMicrophonePermissionStatus`, `openOsPermissionSettingsMenu`, `setContextAccessIndicator` (the "sharing" glow), closed captions, `setAudioDucking` |
| actor | `createTask`, `performActions` (a serialized `Actions` proto), `cancelActions`, `stopActorTask`, `pauseActorTask`, `resumeActorTask`, `interruptActorTask`, `getActorTaskState`, `createActorTab`, `getContextForActorFromTab`, plus confirmation-dialog request handlers (credentials, autofill, navigation, Gmail OTP) |
| other | `getOsHotkeyState`, `openGlicSettingsPage`, `openPasswordManagerSettingsPage`, `registerConversation`, `switchConversation`, `getZeroStateSuggestionsForFocusedTab`, `skills()`, `getChromeTools` / `executeTool`, `getMetrics`, `getJournalHost`, `setSyntheticExperimentState`, onboarding state |

`TabData` is `{tabId, windowId, url, title?, faviconUrl?, documentMimeType?,
isActiveInWindow?, isWindowActive?, ...}`; ids are strings.

### How Chrome serves it

- **Focused tab.** `GlicFocusedTabManager` (`host/context/`) follows the
  focused browser window's active tab (`GlicFocusedBrowserManager`). A tab is
  "focused" only when its window is the focused one and
  `IsTabValidForSharing` (http/https/file, not a Chrome page); otherwise the
  client gets `hasNoFocus` with the candidate tab. Changes go to the client as
  `WebClient.NotifyFocusedTabChanged`.
- **Context.** `glic_page_context_fetcher.cc` maps `TabContextOptions` onto
  `page_content_annotations::FetchPageContextOptions` and calls
  `page_content_annotations::FetchPageContext`: inner text up to the byte
  limit, a viewport screenshot (`ScreenshotOptions::ViewportOnly`), PDF bytes
  from the PDF viewer, and the annotated page content tree (blink's
  `AIPageContentAgent`) when asked. Media pages add a transcript node
  (`GlicMediaIntegration`).
- **Multi-tab.** `GlicPinnedTabManagerImpl` keeps up to 10 pinned tabs by
  default (`kDefaultMaxPinnedTabs`; the client can raise it). Pinned tabs are
  the "@tab" mentions; `getContextFromTab` fetches any of them with the same
  fetcher. Pin candidates are the profile's open tabs, ranked by
  `GlicPinCandidateComparator` and filtered by `query`.
- **Actor.** `actor::ActorKeyedService` runs tasks; `performActions` carries
  click, type, scroll, navigate and tool calls into the page through the
  actor's tool framework, with the tab indicator, border glow and handoff UI
  (`browser_ui/`).

### The rest of the feature set

Voice (Gemini Live, microphone permission and ducking), region capture and
screenshots, "scroll to citation" highlighting (`glic_annotation_manager`), the
PDF summarize button, contextual cue chips and zero-state suggestions, the
"Ask Gemini" context menu on a selection (`glic_selection_*`), skills, the
actor (agentic browsing), conversation restore per tab, and enterprise
(Gemini Enterprise) settings. Each one is a `GlicBrowserHost` method or
observable from the table above plus browser-side UI.

## 4. Is glic in Edge? [disasm]

Searched in the `msedge.dll` public PDB (`*glic*`, `*Glic*`, 390 symbols):

- **Absent:** `GlicKeyedService*`, `GlicButton*`, `GlicButtonController`,
  `GlicWindowController`, `GlicUI`, `glic::Host`, `GlicPageHandler`,
  `GlicWebClient*`, `GlicSharingManager*`, `GlicFocusedTabManager`,
  `GlicPinnedTabManager*`, `GlicEnabling::*`, `GlicGlobalEnabling` and any
  `glic::mojom::` interface. No `persist:glicpart`, `X-Glic` or the glic
  WebUI resources.
- **Present, shared code only:** the actor framework
  (`actor::ActorKeyedService`, `0x96468da` `CreateTask`), contextual cueing
  (`glic::ContextualCueingService`), blink's `GlicMarker` highlight, the glic
  feature constants (`kGlicActor`, `kGlicZeroStateSuggestions`, ...), the
  `chrome://glic/*` URL patterns in an allow-list, and
  `page_content_annotations::FetchPageContext` (`0x8751780`), which Edge uses
  for its own features. `autofill::AutofillClient::OpenGeminiInSidebar` is
  an empty stub (`0x5466b60`, shared with other no-ops).

So nothing can be "enabled": the browser half of glic was not compiled into
Edge.

## 5. Edge's Copilot button and pane

### The button [disasm] [run]

The toolbar shows a split button: the Copilot logo with the label "Chat" and
a chevron. [run]

| piece | class | notes |
|---|---|---|
| container | `EdgeCopilotToolbarSplitButtonContainer` | the pill holding both buttons |
| main button | `EdgeCopilotToolbarLabelButton` (derives from `EdgeDiscoverToolbarButton`, a `ToggleToolbarButton`) | ctor `0x31f66da`, vftable `0x1136ba20` |
| chevron | `EdgeCopilotDropdownMenuButton` | `ButtonPressed` `0xdaf2eb0` opens the Copilot menu |
| behaviour | `EdgeDiscoverToolbarButtonActionEdgeCopilot` | held by the button at `+0xa20` |

- **Label.** `EdgeCopilotToolbarLabelButton::UpdateLabelFromController`
  (`0xc799e6c`) reads the tab's nudge signal from
  `edge_copilot::CopilotButtonLabelController` (`+0xb68`); with no signal it
  uses string `0x46f8` ("Chat") and sets the text, caching it at `+0xb70`.
- **Image.** `EdgeDiscoverToolbarButton::SetImages` (`0x2e25826`) asks the
  action for `GetNormalStateImage` (`0x2a93360`) and `GetPressedStateImage`
  (`0x3407240`), each a `gfx::ImageSkia` from the resource bundle (the bitmap
  Copilot logo), wraps them with `ui::ImageModel::FromImageSkia` and sets
  them per button state.
- **Tooltip and name.** The action's `GetButtonTooltipText` (`0x3407230`,
  string `0x134`) and `GetAccessibleName` (`0xdac0660`, string `0x5160`).
- **Click.** `EdgeCopilotToolbarLabelButton::ButtonPressed` (`0xc79a0d0`)
  first runs a pending nudge action, if any, else
  `EdgeDiscoverToolbarButton::ButtonPressed` (`0xc798680`), which calls the
  action's vtable `+0x18`, `EdgeDiscoverToolbarButtonActionEdgeCopilot::OnClick`
  (`0xdac0670`) -> `BaseEdgeDiscoverToolbarButtonAction::ToggleDiscoverChatSidePane`
  (`0xdaf2860`).

### The pane [run] [disasm]

Clicking it opens Edge's side pane on the right. In CDP the pane is a
`browser_ui` target `edge://discover-chat-v2/` holding an iframe
`https://copilot.microsoft.com/`. [run] The WebUI controller is
`edge::copilot::controller::CopilotUI<...>`, a template over about twenty
mojo "UIBase" mixins (chat, auth, page context, vision, tools, actions,
files, ...). A Microsoft 365 variant (`CopilotExternalSidePanelContents`)
navigates a plain WebContents to `edge::copilot::kM365CopilotChatAppCrURL`,
guarded by `CopilotExternalSidePanelNavigationThrottle::CheckHost`
(`0x915ba28`, `edge::copilot::IsChatAppUrl`), which also attaches a Microsoft
token to requests. [disasm]

### How Copilot gets page context [disasm]

Through the WebUI's `edge::copilot::page_context::PageContextUIBase`
(`CreatePageContextPageHandler`), not through an extension or a web API:

- `PageContextHandler` follows the active browser (`OnActiveBrowserChanged`)
  and serves `GetDocumentInfo`, `GetDocumentBody`, `GetDocumentMetaData` and
  `GetDocumentInfoForTab` (a specific tab), with DLP checks
  (`CaptureDlpPolicyState`) and unfreezing discarded tabs
  (`TabUnfreezeManager`).
- `ChatExtractionHandler` does the work: `ExtractTextFromWeb`,
  `ExtractTextFromPDF`, `ExtractTextFromGoogleDocs`,
  `ExtractTextFromReaderMode`, `ExtractVideoTranscript`, each by
  `ExecuteJavaScriptInIsolatedWorld` in the page.
- `FindOnPageHandler` highlights cited text (`HighlightTextOnPage`).

These answer the Copilot WebUI's mojo pipes only; nothing exposes them to
another origin.

## 6. What this allows, and the design fix-edge uses

Enabling dormant glic is impossible (section 4), and the Copilot pane's
plumbing is bound to its WebUI and origin. The least invasive design that
still gives Gemini the Chrome experience:

1. **Reskin, by hooking the action, not the view.** With Gemini picked, the
   five action functions return the Gemini icon, "Gemini" as tooltip and
   accessible name, and `OnClick` toggles Gemini instead of Copilot; the
   "Chat" label becomes "Gemini". Copilot picked = the originals run.
2. **Host the real client in a side panel of our own.** A `views::WebView`
   (`WebView(BrowserContext*)`, `LoadInitialURL`) loads
   `https://gemini.google.com/glic` in the profile's own storage, so a user
   signed in to Google in Edge is signed in there. It sits at the right of the
   window's contents, laid out by the existing `MultiContentsView` layout
   detour, and stays put while tabs change, as Chrome's panel does.
3. **Be the glic host.** The DLL runs a script in an isolated world of the
   Gemini page (`RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld`) that
   dispatches the `glic-bootstrap` message the client waits for. A
   `MessageEvent` built in script may name any origin, and an event's data is
   cloned across worlds, so the client's own listener accepts it and runs our
   API library in the page's main world with its Trusted Types policy. The
   library implements `GlicBrowserHost` for the methods that matter and
   answers the rest like the client's own fake host.
4. **Tab context from the DLL.** The library's requests travel to the
   isolated world by `postMessage` and on to the DLL as tagged console
   messages, caught in `WebContentsImpl::DidAddMessageToConsole`; answers come
   back by running script in the page again. The DLL knows the window's tabs
   (tab strip model): it reports the active tab (focus), lists tabs for
   pinning, and reads a tab's text by running an isolated-world script in
   that tab, whose console reply it forwards to Gemini.

What this cannot match: anything Chrome does in the browser process that has
no page-level equivalent (viewport screenshots, PDF bytes, annotated page
content, the actor, the OS hotkey, cookie sync into a separate partition), and
anything Google gates on its side (a signed-in, eligible account; the server
may also refuse a client it does not recognise as Chrome). Section 7 records
what the implementation does and what was verified.
