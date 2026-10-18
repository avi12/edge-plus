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
  out it shows "Gemini isn't available right now"). Section 9 works out what
  that takes and what fix-edge now provides (a JavaScript host for Chrome's own
  client library); section 6 is the fallback, the regular web app.
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
decides server-side who may use it. The first version did not imitate that
host (section 9 later does, still leaving Google's own account checks to
Google). What that version did, and still does as the fallback:

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

The limit, stated plainly (as first shipped): this is the Gemini web app
beside the page, not Chrome's Gemini. Sections 8 and 9 supersede it: the panel
can now share tabs with the web app, and hosts Chrome's own glic client where
Google serves it. Section 7 records what was implemented first.

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
- **Tab context and multi-tab** were not wired in this first version; see
  sections 8 and 9.
- The panel UI itself was not launched live, because running the real panel
  writes the shared config file and signals the shared named events, which
  must not be touched while the user's real Edge is listening; it is type- and
  lint-clean and mirrors the existing cards.
- The docked panel overlaid the right of the page (fixed: section 10).

## 8. Sharing the current tab and other tabs with the web app [run]

The fallback mode, and the one that works signed out. A slim strip (our own page,
loaded as a `data:` URL in a second `views::WebView` above Gemini's page) holds
"Share this tab" and "Pick tabs". It cannot call the DLL, so a UI-thread timer
(150 ms, only while a panel is open) asks it for its clicks and answers.

- **Reading a tab.** `RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld`
  (`0x78d3d90`: `this`, `const u16string*`, `OnceCallback<void(Value)>*`, world id
  1..11). The main-world `ExecuteJavaScript` is unusable: it `CHECK`s
  `CanExecuteJavaScript` (`0x78d3b2a`), which a web page fails. The callback is a
  `BindStateBase` we lay out ourselves (`BindStateBase::BindStateBase`, `0xf94c70`:
  refcount 1 at +0, invoke at +8, destructor at +0x10, bound state from +0x28); the
  reply `base::Value` is read in place (variant index byte at +0x18, 4 = string;
  libc++ strings use the alternate layout). The title is `WebContentsImpl::GetTitle`
  (`0x10ca640`), the address `GURL::spec` (`0xf9fc3a`). Unload waits for
  outstanding callbacks, or stays loaded, so no reply can jump into a freed DLL.
- **What is shared.** Address, title, selection and `innerText`, cut at 30,000
  characters with a visible note, inserted into Gemini's prompt box with the editing
  command a paste uses. Sharing never sends. `edge://` pages, asleep (discarded)
  tabs and PDFs are refused with a message. Several tabs can be added at once from
  the picker.
- **Not done.** Screenshots (would need `CopyFromSurface` plus PNG encoding) and
  Edge's own `ChatExtractionHandler` (PDF, Google Docs, reader mode, transcripts).
- **Verified** in a throwaway Edge, signed out: tab text into the prompt, a
  four-tab picker with Edge pages marked "Can't be shared", three tabs added in one
  go (8,770 characters), read back through CDP.

## 9. The real glic host: what is feasible [src] [run] [disasm]

Question: can fix-edge host Chrome's glic client (`gemini.google.com/glic`) so the
Gemini panel is the same as Chrome's?

**What the client checks.** Only this, in the Gemini page's bundle: a `message`
event with `origin === "chrome://glic"`, `source !== null` and
`data.type === "glic-bootstrap"`. It then runs `data.glicApiSource` as a script
(through a Trusted Types policy) and calls `window.internalAutoGlicBoot`. The check
is on the event's `origin` property, so a `MessageEvent` dispatched from our
isolated world with `origin: "chrome://glic"` and `source: window` is accepted.
There is no client-side check of user agent, client hints, cookies or the `X-Glic`
request headers. [run]

**The library.** Chrome's `glicApiSource` is read from an installed Chrome's
`resources.pak` (public Program Files; 155.0.8059.85: a gzip resource of 485,656
bytes holding `internalAutoGlicBoot`). Edge's own zlib (`GzipUncompressHelper`,
`0x2a53484`) and brotli (`BrotliDecoderDecompress`, `0x1ebef00`) decode it; it is
held in memory only. [run]

**Why not a postMessage host.** In Chrome 155 that script is client and host in
one: `InFrameGlicBoot` builds the `GlicApiHost`, which speaks mojo
(`glic.mojom.WebClientHandler`, bound through the page's global `Mojo`) straight to
Chrome's browser process. Edge has no such service (section 4) and a web page has no
`Mojo` global. [src] [run]

**What fix-edge does instead** (`glic_host_script.h`). The patcher renames the
library's generated mojo `*Remote` / `*Receiver` classes and puts stand-ins first
(the handler remote becomes a JavaScript object answering the handler's methods; a
receiver just hands its implementation to whoever asks for its pipe). The host
answers what Chrome's browser answers, from the tabs of the Edge window, over a DOM
bridge the DLL polls (the one thing the page's main world and our isolated world
share): `webClientCreated` (initial state), the focused tab and its changes,
`getContextFromFocusedTab` / `getContextFromTab` (title, address, origin, text), pin
candidates and `pinTabs` / `unpinTabs` / `setMaximumNumberOfPinnedTabs`,
`activateTab`, and `invoke` for a summary. Every other handler method is accepted
and ignored.

**Verified (signed out), against Chrome's real library and Google's real page:**

- the page's own bootstrap listener took our message and the library booted;
- a test web client registered the way the app does got: Chrome version and
  platform, the focused tab (`hasFocus`), the focused tab's text, another tab's text
  by id, the window's pin candidates, and pinned tabs;
- the Gemini app's own client, when it registered, reached the host (its mojo calls
  arrived: permission state, profile, synthetic experiment state). [run]

**What blocks it, with evidence.** Signed out, `/glic` renders "Gemini isn't
available right now" before and after a host answers, and also with Google's own
built-in `?glic=fake` development host. So the gate that decides whether glic is
served is the account or the server (Google's `glicStatus` and the sign-in), which
cannot be seen signed out and was not tried signed in. The panel therefore tries
glic first and, when the page says it is unavailable, the client never registers
within 15 s, or `initialize` fails, falls back to the web app and strip of section 8
in the same page. With no Chrome installed it never tries glic.

**Not identical to Chrome** (not done, or not possible):

- no viewport screenshot (`viewportScreenshot` is null), no annotated page content
  (blink's `AIPageContentAgent`; Edge only has `FetchPageContext` at `0x8751780`),
  no PDF bytes, no scroll-to-citation, no actor or agentic browsing, no zero-state
  suggestions, no skills, no favicons, no region capture;
- `createTab` answers "no tab"; microphone and location permission go through Edge's
  own prompt, not Chrome's plumbing; "Talk" cannot start voice in glic (the client
  starts it from its own button);
- Chrome's panel chrome (header, detach to a floating window, resize handle) is not
  reproduced; the panel is a fixed 420 DIP dock;
- the host is JavaScript in the page, so it lives as long as the page; a Chrome
  whose library changes shape breaks the patcher (it fails safe: the client never
  registers and the panel falls back).

## 10. Making room for the panel, the chat dropdown, windows

- **Real space.** The panel used to overlay the page. The layout detour of
  `MultiContentsView` now hands Edge narrower bounds by the width an open panel
  reserves (`reserve.h`), and the split's content box subtracts it, so the page and
  every pane reflow beside the panel on open, close and resize. The detour stays
  enabled with the split off. Not done: Chrome's resize handle, and Edge's own
  side-pane slot (a `SidePanelEntry` needs hand-made callbacks and views; the
  reserved-width detour gives the same reflow with what the DLL already has).
- **A pending Copilot nudge** makes `EdgeCopilotToolbarLabelButton::ButtonPressed`
  (`0xc79a0d0`) run the nudge instead of `OnClick`, which opened Copilot's pane under
  the Gemini button; with Gemini chosen the press is always the click.
- **New windows** get the Gemini label from `OnThemeChanged` (`0x2e255b0`), which
  runs when a button is attached and themed.
- **Closed windows.** A panel remembers its window and is dropped untouched once the
  window or its `MultiContentsView` is gone (the window already destroyed the
  views). Not reproduced live: a second window cannot take a posted click without
  becoming the foreground window.
- **The dropdown.** `EdgeCopilotMenuModel::BuildMenu` (`0xf04e1f4`) runs on each
  open, so while Gemini is chosen its voice item (command 1) is renamed "Talk to
  Gemini + Share tab" after the build and Edge's text returns by itself with
  Copilot. `ExecuteCommand` (`0xf04e420`): Chat (0) opens the Gemini panel; Summarize
  page (3) opens it, shares the tab and presses send; Talk (1) opens it, shares the
  tab and presses Gemini's voice button once it exists. Nudges stay Edge's. Verified
  with UI Automation (item names `Summarize page`, `Chat`, `Talk to Gemini + Share
  tab`) and a capture of the open menu; Summarize page produced a Gemini summary of
  the shared page signed out. The voice button is signed-in only, so Talk was
  verified up to the shared prompt.

## 11. The button's right-click menu ("Hide Copilot")

- **Where the text comes from.** `EdgeToolbarView::CreateCopilotButtonContextMenu`
  (`0x31f6466`) makes a `ToolbarMenuButtonContextMenu` with "hide button" command id
  `0x9e67` (or `0xcea4` when `IsHideCopilotNavigatesToSettingsEnabled`). Its
  constructor calls `AddHideButtonItem` (`0x2a3aee0`), which for those two ids adds
  the item with string `0x4562` ("Hide Copilot"); other toolbar buttons get other ids
  and strings, so nothing else shares it. It is a fixed string, not formatted with the
  Copilot name. The menu is built **once**, with the toolbar, and its
  `ui::SimpleMenuModel` stays at `[menu+0x18]`; the menu is the button's
  `ToolbarButton::SetContextMenu` pointer at `[button+0x710]`. The menu has no other
  Copilot-named item (the rest are the generic toolbar commands).
- **The hook.** Hooking the builder would miss a switch made after the toolbar exists,
  so the item is set on each right-click instead: `ToolbarButton::ShowContextMenuForViewImpl`
  (`0xa2c6780`, `this` = the ContextMenuController subobject, button + `0x6f0`). When
  that view is the chat button and the menu has the hide item, its label is set to
  "Hide Gemini" with Gemini chosen, else back to string `0x4562`. Every other button
  passes straight through.
- **Verification.** Static only; not exercised live.

## 12. The missing chevron beside the chat button (Copilot Mode) [disasm] [run]

- **Symptom.** In a profile with Copilot Mode on (`edge_copilot_mode_enabled` and
  `edge_copilot_mode_enabled2` true in Preferences) the toolbar's chat pill has no
  chevron; a fresh profile has it. Our DLL never hid it.
- **Where it is decided.** The chevron's `EdgeCopilotToolbarSplitButtonContainer` is
  built in one place, `EdgeToolbarView::CreateCopilotSplitButtonContainer` (`0x31f62fc`),
  called only from `EdgeToolbarView::CreateDiscoverButtonIfNecessary` (`0x31f5e50`) when
  `edge_copilot::utils::IsCopilotLabeledButtonWithDropdownEnabled(profile, true)`
  (`0x31f600a`) is true. Otherwise the same function builds the bare
  `EdgeCopilotToolbarLabelButton` (the template at `0xa2e082a`). Both paths build the
  same button class, so a bare chat button is indistinguishable by vtable.
- **The predicate.** For a profile with Copilot available, not guest/off-the-record:
  AAD profile -> feature `CommercialCopilotLabelButtonWithDropdown` (default off);
  else if `GetProfileSupportResultForChatV2Launch` (`0x2682f7e`) is non-zero -> false;
  else `IsConsumerCopilotLabelButtonWithDropdownEnabled` (`0x31f6048`), a thin wrapper
  over `base::FeatureList::IsEnabled` of `ConsumerCopilotLabelButtonWithDropdown`
  (default **enabled**; the feature object is `kCopilotOmniboxEntryPoint+0x28`,
  referenced only by that wrapper).
- **The condition.** The profile here is a personal MSA one, so the consumer feature
  decides. With the two Copilot Mode prefs set in an otherwise fresh throwaway
  profile the chevron disappears; launching the same profile with
  `--enable-features=ConsumerCopilotLabelButtonWithDropdown` brings it back. So it is
  Edge's own behaviour for Copilot Mode (the feature reads as off, though its default
  is on; the exact override was not traced) and not something the DLL or the panel
  did. Nothing in src/ references the feature or the container.
- **The fix.** `assistant.h` hooks the wrapper (`0x31f6048`, 14 bytes of
  `mov/lea/jmp`) and answers "yes" while Gemini is the chosen assistant; with Copilot
  chosen it calls Edge's own answer. It is read when a toolbar is built, so it
  applies to windows opened after the DLL has the setting (an already-built toolbar
  is not rebuilt).
- **Verification.** Live in a throwaway Edge (marker `fixedge-chev-`, Copilot Mode
  prefs set, Gemini chosen via the config file, DLL injected): the first window
  keeps its bare button; a new window (`IDC_NEW_WINDOW`) shows "Ask Gemini" with its
  chevron. Captured with `PrintWindow`.

## 13. The page's right-click "Ask Copilot" items [disasm] [run]

- **Where the items are built.** `RenderViewContextMenu` (rebuilt on every
  right-click; its own `ui::SimpleMenuModel` is embedded at `this+0x5c0`) adds them
  in three builders. `AppendAskCopilotInPageContextualMenu` (`0xaecb048`) tail-jumps
  to `AddEdgeCopilotContextMenuItemsInL2` (`0xaecbe96`) when the profile has contextual
  podcast (`IsContextualPodcastEnabledForProfile`), else to `...InL1` (`0xaecbfea`).
  L1 adds one item, command `0xcd71`, string `0x44e9` ("Summarize with Copilot").
  L2 adds the **"Ask Copilot" submenu** (command `0xcd76`, string `0x44f7`, its model
  at `this+0x1170`) holding Summarize (`0xcd71`, `0x44ef`) and Create podcast
  (`0xcd77`, `0x44ec`; also `0xcd77` for PDFs). `AppendChatItem` (`0xaecb4ae`, with
  `QuickSearchClientImpl::AddChatMenuItemForWeb`) adds, for a selection or a PDF, a
  separate submenu of the same name (`0xcd68`, or `0xcd69` on a PDF; string
  `0x44f7`) whose items `0xcd6a..0xcd6d` (strings `0x44e5..0x44e8`) open Copilot's
  chat.
- **What Copilot does.** Summarize (`0xcd71`) and Create podcast (`0xcd77`) both run
  `RenderViewContextMenu::SendPageContextualPromptToChat` (`0xaed4b2c`; `this`,
  command id): it sends a canned prompt (strings `0x44ea`
  title / `0x44e9` prompt for summarize, `0x44ed` / `0x44ec` for the podcast, source
  tag `EDGTCXPD` for the podcast) with the page to **Copilot's side pane** via
  `edge_copilot::utils::SendPageContextualPromptToChat` (or the commercial
  variant). So the podcast is a prompt to Copilot's chat about the current page, not
  a separate window; the audio is Copilot's reply there. The selection items go
  through `EdgeExecCopilotItem` (`0xaed0ec6`) and `QuickSearchClientImpl::OpenChat`
  with the command id.
- **What fix-edge does with Gemini chosen** (`page_menu.h` is the one table).
  After each of the three builders runs (hooks in `assistant.h`), the top-level
  items are relabelled ("Ask Gemini", "Summarize with Gemini") or removed (the
  selection submenus). `SendPageContextualPromptToChat` is hooked: Summarize runs
  the same action as the chat dropdown's Summarize (`chat_actions::Run`, share the
  tab, press send / glic `invoke`), Create podcast runs `ChatAction::kPodcast`: the
  same path with the prompt "Create a podcast (an Audio Overview) of this page" (a
  `chat_action.h` constant). The submenu's children keep Edge's text (it names no
  assistant). With Copilot chosen every hook calls Edge's own code.
- **Verified live** (throwaway Edge, marker `fixedge-ctx-`, own `LOCALAPPDATA` with
  Gemini chosen, DLL injected; right-click posted to the page's render widget, items
  read through UIA): the page menu shows "Summarize with Gemini" (L1 variant); invoking
  it opened the Gemini panel, shared the page and Gemini answered with a summary
  (captured with `PrintWindow`). With Copilot chosen the same menu reads "Summarize
  with Copilot" (Edge's text, pass-through).
- **Not verified.** (1) The L2 submenu with Create podcast and the selection submenu
  never appear in a fresh personal profile (podcast and chat-prompt eligibility), so
  the relabel of `0xcd76`, the removal of `0xcd68`/`0xcd69` and the podcast prompt
  were not seen live; they use the same code path as the verified L1 relabel and
  Summarize run. (2) Whether Gemini returns audio for the podcast prompt in the web
  app or the glic client: Audio Overview is a NotebookLM feature; the prompt is sent
  and what Gemini does with it is Google's. (3) Selection items: the selection text
  (`this+0x1d0`, a `u16string`, from `AppendSearchProvider`) could be sent as a
  prompt, but the panel's web-app path can only insert shared pages, so Gemini
  selection prompts are a gap; the selection submenu is hidden with Gemini rather
  than opening Copilot.
- **Windows already open.** The hook above only reaches toolbars built afterwards, so
  `assistant::Refresh` also rebuilds an open window's chat button: the chat button's
  owner is the ancestor whose `[+0x5d0]` is the button (`EdgeToolbarView`; `+0x5a8` is
  the base `CreateDiscoverButtonIfNecessary` takes). Calling it again builds the
  container and chevron (it does not check for an existing button, and stores the
  new button at `[+0x5d0]`); the old bare button is then detached with
  `View::RemoveChildView` and never deleted, since other Edge objects may still point
  at it. Choosing Copilot again hides the chevron unless Edge's own answer is yes;
  choosing Gemini shows it. Verified live on 155.0.4283.45 with one switch (the
  existing window showed "Ask Gemini" and its chevron, and stayed responsive). Not
  verified after the last tweak (the toolbar's layout is invalidated so the label is
  not clipped) nor with repeated switching: Edge updated to .62 mid-task and the
  DLL's build check (rightly) refuses it, so no .45 was left to test against.
