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
- **The glic client is bound to Chrome's host.** `gemini.google.com/glic`
  only starts its in-browser mode on a message from origin `chrome://glic`,
  and Google's server then decides whether the account may use it (signed
  out it shows "Gemini isn't available right now"). fix-edge does not
  imitate that host: section 6 hosts the regular Gemini web app instead.
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
   which returns a `GlicHostRegistry`.
3. The client calls `registry.registerWebClient(client)`; the host calls
   `client.initialize(browserHost)` and later
   `client.notifyPanelWillOpen({kind: ATTACHED, ...})`.
4. Underneath, the library speaks a request/response protocol over
   `postMessage` to the WebUI (`glic_api_impl/transport/`), which forwards
   each request to the browser over the mojo interface `glic.mojom.WebClientHandler`
   (`host/glic.mojom`); `glic.mojom.WebClient` carries notifications back.

Loaded directly in a throwaway Edge, `https://gemini.google.com/glic` shows
the panel chrome and "Something went wrong / Gemini isn't available right
now": no Chrome host, and no signed-in account. [run]

### The `GlicBrowserHost` surface

From `glic_api.ts` (most methods optional; an `Observable` is `{subscribe(cb), subscribeObserver?(o)}`
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
plumbing is bound to its WebUI and origin. Chrome's in-browser Gemini mode
belongs to Chrome: its client only starts on Chrome's own host, and Google
decides server-side who may use it. fix-edge does not imitate that host or
work around Google's checks. What it does:

1. **Reskin, by hooking the action, not the view.** With Gemini picked, the
   action's image, tooltip and accessible name become Gemini's and `OnClick`
   toggles Gemini instead of Copilot; the "Chat" label becomes "Gemini".
   Copilot picked = the originals run.
2. **Host the regular Gemini web app in a side panel of our own.** A
   `views::WebView` (`WebView(BrowserContext*)`, `LoadInitialURL`) loads
   `https://gemini.google.com/app` with the profile's own storage, so a user
   signed in to Google in Edge is signed in there. It sits at the right of
   the window's contents, laid out by the existing `MultiContentsView` layout
   detour, and stays put while tabs change, as Chrome's panel does.

The limit, stated plainly: this is the Gemini web app beside the page, not
Chrome's Gemini. It does not read the active tab, pin other tabs, act on
pages, or use the microphone through Chrome's permission plumbing; the user
shares a page the way the web app allows (paste, upload). Section 7 records
what was implemented and verified.

## 7. What shipped, and how it was verified

The implementation is the design above, built into the existing injected DLL
(no new process) and driven by the panel's config file.

- **DLL** (`app/native-split/`): `assistant.h` hooks the chat-action's
  `GetNormalStateImage` / `GetPressedStateImage` (`0x2a93360` / `0x3407240`),
  `GetButtonTooltipText` (`0x3407230`), `GetAccessibleName` (`0xdac0660`) and
  `OnClick` (`0xdac0670`), plus the button's `UpdateLabelFromController`
  (`0xc799e6c`). While Copilot is chosen every hook runs the original, so
  Edge's button and pane are byte-for-byte untouched. While Gemini is chosen
  the image is the spark from `icons.h`, the tooltip/name/label are "Gemini",
  and the click toggles a `gemini.h` panel: a `views::WebView` on the window's
  profile, docked over the right of the content, loading
  `https://gemini.google.com/app`. On a config change the live button is
  re-skinned by walking the window's view tree for the button
  (`CopilotButton_vftables`); on unload the button is restored and the panel
  removed before the hooks come off. The reskin is independent of the split,
  so turning the split off does not un-reskin the button.
- **Panel** (`app/panel/`): a `Browser assistant` card (`AssistantCard.svelte`,
  a `Select` of Copilot / Gemini) → `set_assistant` → `settings.features.assistant`
  → the config wire's assistant field (`protocol.rs`, mirroring `config.h`).
  The DLL stays loaded while Gemini is selected, and `edge_build.rs` registers
  the assistant as depending on the pinned `msedge.dll` build, so an Edge
  update forces it back to Copilot like the split and theming.

Verified in a throwaway Edge (own `--user-data-dir` and `LOCALAPPDATA`, killed
by marker; captured with `PrintWindow`):

- Gemini selected: the toolbar button reads "Gemini" with the spark icon, and
  clicking it opens the Gemini web app docked at the right ("Ask Gemini",
  signed out). Clicking again closes it.
- Copilot selected: the button is the stock "Chat" with the Copilot logo, and
  clicking it opens Edge's own `edge://discover-chat-v2/` Copilot pane — no
  regression.

Open / not done:

- **Signed-in chat** needs a Google sign-in in the panel's partition; not done
  (not signing in with the user's account). The web app loads and prompts to
  sign in.
- **Tab context and multi-tab** are not wired: this is the Gemini web app, not
  Chrome's glic host, so there is no `getContextFromFocusedTab` / pinned-tab
  bridge. The user shares a page the way the web app allows.
- The panel UI itself was not launched live, because running the real panel
  writes the shared config file and signals the shared named events, which
  must not be touched while the user's real Edge is listening; it is type- and
  lint-clean and mirrors the existing cards.
- The docked panel **overlays** the right of the page rather than reflowing it.
