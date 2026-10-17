// glic_host_script.h — the JavaScript of fix-edge's glic host. One concern: the
// browser side of Chrome's "Gemini in Chrome" API, written in JavaScript to run
// inside the gemini.google.com/glic page itself, because Chrome's own host is C++
// in chrome.dll (glic::GlicWebClientHandler, a mojo service) and Edge has none.
//
// In Chrome 155 the glic client library that Chrome hands the page
// ("glicApiSource", from Chrome's resources) is client and host in one script:
// its GlicApiHost talks mojo (`glic.mojom.WebClientHandler`) to the browser and
// is called by the web client through postMessage-shaped plumbing in the same
// frame. So the seam we can take over is the mojo handler: glic_host_patch
// renames the library's generated mojo remotes/receivers and appends stand-ins
// that call this host instead, and this host answers the handler's methods
// (what Chrome's browser answers) from the tabs of the Edge window, over a DOM
// bridge the DLL polls (share.h/glic.h). Two scripts, one concern each:
//   kHostScript   — runs in the page's main world, inside the library script.
//   kPatchScript  — runs in our isolated world: patches and boots the library.
#pragma once

namespace fixedge::glic_host_script {

// The bridge element both worlds share (a DOM node is the one thing a page's
// main world and our isolated world can both see), and the markers the main
// world leaves on <html> for the DLL to read.
inline constexpr char kBridgeId[] = "fixedge-bridge";

// The host, in the main world. It is prepended to the library, before any of
// it runs. `window.__fixedgeHost` is what the stand-in handler talks to.
inline constexpr char kHostScript[] = R"JS((function () {
  "use strict";
  var BRIDGE_ID = "fixedge-bridge";
  var ANSWER_TIMEOUT_MS = 8000;
  var MAX_PINNED_TABS = 10;
  var PANEL_ATTACHED = 2;
  var PLATFORM_WINDOWS = 2;
  var FORM_FACTOR_DESKTOP = 1;
  var HOST_CAPABILITY_MULTI_INSTANCE = 3;
  var CHROME_VERSION = [155, 0, 8059, 85];
  var NO_FOCUS_REASON = "The tab cannot be shared.";

  var root = document.documentElement;
  function bridge() {
    var element = document.getElementById(BRIDGE_ID);
    if (!element) {
      element = document.createElement("div");
      element.id = BRIDGE_ID;
      element.hidden = true;
      root.appendChild(element);
    }
    return element;
  }

  // ---- calls to the DLL (the Edge side) ---------------------------------------
  var pending = new Map();
  var nextCallId = 1;
  function call(operation, args) {
    return new Promise(function (resolve, reject) {
      var id = nextCallId++;
      var timer = setTimeout(function () {
        pending.delete(id);
        reject(new Error("Edge did not answer " + operation));
      }, ANSWER_TIMEOUT_MS);
      pending.set(id, { resolve: resolve, reject: reject, timer: timer });
      var request = document.createElement("i");
      request.dataset.id = String(id);
      request.dataset.op = operation;
      request.textContent = JSON.stringify(args === undefined ? null : args);
      bridge().appendChild(request);
    });
  }
  function onBridgeEvent() {
    Array.from(bridge().children).forEach(function (node) {
      if (node.tagName === "B") {
        var entry = pending.get(Number(node.dataset.id));
        node.remove();
        if (entry) {
          pending.delete(Number(node.dataset.id));
          clearTimeout(entry.timer);
          var answer = JSON.parse(node.textContent);
          if (answer.ok) {
            entry.resolve(answer.value);
          } else {
            entry.reject(new Error(answer.error || "failed"));
          }
        }
      } else if (node.tagName === "U") {
        var kind = node.dataset.kind;
        var payload = node.textContent ? JSON.parse(node.textContent) : null;
        node.remove();
        onPush(kind, payload);
      }
    });
  }
  bridge().addEventListener("fixedge-reply", onBridgeEvent);

  function mark(name, value) {
    root.dataset[name] = value;
  }

  // ---- Chrome's mojo shapes from Edge's tab data ------------------------------
  // A tab as the DLL describes it: {tabId, windowId, url, title, isActive}.
  function tabData(tab) {
    return {
      tabId: tab.tabId,
      windowId: tab.windowId,
      url: tab.url,
      title: tab.title || null,
      favicon: null,
      faviconUrl: null,
      documentMimeType: "text/html",
      isObservable: true,
      isMediaActive: null,
      isTabContentCaptured: null,
      isActiveInWindow: !!tab.isActive,
      isWindowActive: true,
      lightweightPageFeatures: null,
    };
  }
  function focusedTabData(state) {
    if (state && state.focused) {
      return { focusedTab: tabData(state.focused) };
    }
    return {
      noFocusedTabData: {
        activeTabData: state && state.active ? tabData(state.active) : null,
        noFocusReason: (state && state.reason) || NO_FOCUS_REASON,
      },
    };
  }
  function origin(url) {
    try {
      var parsed = new URL(url);
      return {
        scheme: parsed.protocol.replace(":", ""),
        host: parsed.hostname,
        port: parsed.port ? Number(parsed.port) : 0,
        nonceIfOpaque: null,
      };
    } catch (error) {
      return { scheme: "", host: "", port: 0, nonceIfOpaque: null };
    }
  }

  // ---- the host's state ----------------------------------------------------------
  var host = {
    webClient: null,  // the library's GlicBrowserHostImpl, as the mojo WebClient
    pinned: [],       // tab ids the web client pinned
    pinObservers: [], // {observer, options}
    lastState: null,
  };

  function pinnedTabs(tabs) {
    return tabs.filter(function (tab) {
      return host.pinned.indexOf(tab.tabId) >= 0;
    }).map(tabData);
  }

  function notifyPinCandidates(tabs) {
    host.pinObservers.forEach(function (entry) {
      var query = ((entry.options && entry.options.query) || "").toLowerCase();
      var limit = (entry.options && entry.options.maxCandidates) || tabs.length;
      var candidates = tabs.filter(function (tab) {
        return tab.canShare && (!query ||
          (tab.title + " " + tab.url).toLowerCase().indexOf(query) >= 0);
      }).slice(0, limit).map(function (tab) {
        return { tabData: tabData(tab) };
      });
      entry.observer.impl.onPinCandidatesChanged(candidates);
    });
  }

  function onPush(kind, payload) {
    if (!host.webClient) {
      return;
    }
    if (kind === "state") {
      host.lastState = payload;
      host.webClient.notifyFocusedTabChanged(focusedTabData(payload));
      host.webClient.notifyPinnedTabsChanged(pinnedTabs(payload.tabs || []));
      notifyPinCandidates(payload.tabs || []);
    }
  }

  async function currentState() {
    try {
      host.lastState = await call("state", null);
    } catch (error) {
      host.lastState = null;
    }
    return host.lastState;
  }

  async function initialState() {
    var state = await currentState();
    return {
      panelState: { kind: PANEL_ATTACHED, windowId: 1 },
      focusedTabData: focusedTabData(state),
      microphonePermissionEnabled: false,
      locationPermissionEnabled: false,
      tabContextPermissionEnabled: true,
      osLocationPermissionEnabled: false,
      zoomFactor: 1,
      hotkey: "",
      chromeVersion: { components: CHROME_VERSION },
      platform: PLATFORM_WINDOWS,
      formFactor: FORM_FACTOR_DESKTOP,
      canAttach: true,
      panelIsActive: true,
      browserIsOpen: true,
      instanceIsActive: true,
      enableZeroStateSuggestions: false,
      enableCachedGetUserProfileInfo: false,
      enableActInFocusedTab: false,
      enableScrollTo: false,
      enableDefaultTabContextSettingFeature: false,
      defaultTabContextSettingEnabled: true,
      closedCaptioningSettingEnabled: false,
      enableMaybeRefreshUserStatus: false,
      enableMultiTab: true,
      enableGetContextActor: false,
      hostCapabilities: [HOST_CAPABILITY_MULTI_INSTANCE],
      enableGetPageMetadata: false,
      enableCaptureRegion: false,
      enableWebActuationSettingFeature: false,
      actuationOnWebSettingEnabled: false,
      canActOnWeb: false,
      enableActivateTab: true,
      enableGetTabById: false,
      enableOpenPasswordManagerSettingsPage: false,
      enableOpenContactInfoSettingsPage: false,
      enableTrustFirstOnboarding: false,
      onboardingCompleted: true,
      enableSkills: false,
      enableGetTabFaviconById: false,
      enableProcessCounterAbuseVerdict: false,
      geminiEnterpriseSettings: null,
      enableGmailOtpOptIn: false,
      enableGmailOtpConfirmation: false,
      fileUploadPolicyState: 0,
    };
  }

  function openingData() {
    return {
      panelState: { kind: PANEL_ATTACHED, windowId: 1 },
      invocationSource: 3,
      promptSuggestion: null,
      autoSend: false,
      skillToInvoke: null,
      recentlyActiveConversations: null,
      conversationInfo: { conversationId: "", conversationTitle: "", clientData: null, turnId: null },
      freOverride: 0,
    };
  }

  async function context(tabId, options) {
    var wantText = !options || options.innerText !== false;
    var answer = await call("context", {
      tabId: tabId,
      innerTextBytesLimit: options && options.innerTextBytesLimit ? options.innerTextBytesLimit : 0,
      wantText: wantText,
    });
    if (!answer || answer.error) {
      return { errorReason: (answer && answer.error) || "The tab could not be read." };
    }
    return {
      tabContext: {
        tabData: tabData(answer.tab),
        webPageData: {
          mainDocument: {
            origin: origin(answer.tab.url),
            innerText: answer.innerText,
            innerTextTruncated: !!answer.truncated,
          },
        },
        viewportScreenshot: null,
        screenshotInfo: null,
        pdfDocumentData: null,
        annotatedPageData: null,
      },
    };
  }

  // What Chrome's browser answers to each handler method that matters here;
  // every other one is accepted and ignored (metrics, settings, resizing).
  var answers = {
    webClientCreated: async function (remote) {
      host.webClient = remote.impl;
      mark("fixedgeClient", "created");
      return { initialState: await initialState() };
    },
    webClientInitialized: function () {
      mark("fixedgeClient", "initialized");
      setTimeout(function () {
        if (host.webClient) {
          host.webClient.notifyPanelWillOpen(openingData());
        }
      }, 0);
    },
    webClientInitializeFailed: function () {
      mark("fixedgeClient", "failed");
    },
    getContextFromFocusedTab: async function (options) {
      return { result: await context(null, options) };
    },
    getContextFromTab: async function (tabId, options) {
      return { result: await context(tabId, options) };
    },
    setMaximumNumberOfPinnedTabs: function (requestedMax) {
      return { effectiveMax: Math.min(requestedMax, MAX_PINNED_TABS) };
    },
    pinTabs: async function (tabIds) {
      tabIds.forEach(function (tabId) {
        if (host.pinned.indexOf(tabId) < 0 && host.pinned.length < MAX_PINNED_TABS) {
          host.pinned.push(tabId);
        }
      });
      var state = await currentState();
      host.webClient.notifyPinnedTabsChanged(pinnedTabs((state && state.tabs) || []));
      return { pinnedAll: true };
    },
    unpinTabs: async function (tabIds) {
      host.pinned = host.pinned.filter(function (tabId) { return tabIds.indexOf(tabId) < 0; });
      var state = await currentState();
      host.webClient.notifyPinnedTabsChanged(pinnedTabs((state && state.tabs) || []));
      return { unpinnedAll: true };
    },
    unpinAllTabs: function () {
      host.pinned = [];
      host.webClient.notifyPinnedTabsChanged([]);
    },
    subscribeToPinCandidates: async function (options, observer) {
      host.pinObservers.push({ observer: observer, options: options });
      var state = await currentState();
      notifyPinCandidates((state && state.tabs) || []);
    },
    activateTab: function (tabId) {
      call("activate", { tabId: tabId }).catch(function () {});
    },
    createTab: async function (url) {
      var tab = await call("create", { url: url });
      return { tabData: tab ? tabData(tab) : null };
    },
    // Chrome answers with the signed-in Google profile; the page's own cookies
    // carry the sign-in here, so this only has to be a profile.
    getUserProfileInfo: function () {
      return {
        profileInfo: {
          avatarIcon: null,
          displayName: "Edge profile",
          givenName: "",
          localProfileName: "",
          email: "",
          isManaged: false,
        },
      };
    },
    syncCookies: function () { return { success: true }; },
    getModelQualityClientId: function () { return { modelQualityClientId: "" }; },
    captureScreenshot: function () { return { result: { errorReason: 0 } }; },
    getOsMicrophonePermissionStatus: function () { return { enabled: true }; },
    setAudioDucking: function () { return { result: true }; },
    shouldAllowMediaPermissionRequest: function () { return { isAllowed: true }; },
    shouldAllowGeolocationPermissionRequest: function () { return { isAllowed: true }; },
    isDebuggerAttached: function () { return { isAttachedToWebview: false }; },
    subscribeToPageMetadata: function () { return { success: false }; },
    getZeroStateSuggestionsForFocusedTab: function () { return { suggestions: null }; },
    switchConversation: function () { return { errorReason: null }; },
    registerConversation: function () { return { errorReason: null }; },
    openPinnedTabPicker: function () { return {}; },
  };

  host.method = function (name) {
    var handler = answers[name];
    return function () {
      var args = arguments;
      return new Promise(function (resolve) {
        resolve(handler ? handler.apply(null, args) : {});
      });
    };
  };
  window.__fixedgeHost = host;
  mark("fixedgeBooted", "1");
})();
)JS";

// What the isolated-world script answers on a poll: a first line
// (glic_text::kPollMarker), then one `name=value` line per marker, then one
// `call<TAB>id<TAB>op<TAB>json` per request the host made; glic_text::ParsePoll
// reads it.
// The patcher and the poll, in our isolated world. `setLibrary` takes the
// host and Chrome's library, patches the library's mojo classes and keeps the
// result; `tryBoot` offers it to the page's glic bootstrap listener (which
// only accepts a message that claims to come from chrome://glic, and runs the
// script it carries) until the page took it; `drain` is what the DLL polls.
inline constexpr char kPatchScript[] = R"JS((function () {
  if (window.__fxg) {
    return "ok";
  }
  var BRIDGE_ID = "fixedge-bridge";
  var root = document.documentElement;
  var state = { patched: "" };

  function bridge() {
    var element = document.getElementById(BRIDGE_ID);
    if (!element) {
      element = document.createElement("div");
      element.id = BRIDGE_ID;
      element.hidden = true;
      root.appendChild(element);
    }
    return element;
  }

  // Every generated mojo remote and receiver in the library is renamed and
  // replaced by a stand-in: a receiver just keeps the object that implements
  // it, and hands that object to whoever asks for its pipe; a remote answers
  // any method with an empty result, except the browser's handler, whose
  // methods are the host's.
  var HANDLER_REMOTE = "WebClientHandlerRemote";
  function patch(hostSource, library) {
    var names = [];
    var renamed = library.replace(/class (\w+?)(Remote|Receiver)\{/g, function (match, base, kind) {
      // The generated mojo classes only: the library's own postMessage plumbing
      // has Receivers too (PostMessageRequestReceiver), and a pending receiver
      // is just a handle the stand-ins never touch.
      var isMojo = base.indexOf("PostMessage") !== 0 && !/Pending$/.test(base);
      if (!isMojo) {
        return match;
      }
      names.push({ name: base + kind, isReceiver: kind === "Receiver" });
      return "class " + base + kind + "__orig{";
    });
    var stubs = names.map(function (entry) {
      if (entry.isReceiver) {
        return "class " + entry.name + "{constructor(impl){this.impl=impl;" +
          "this.onConnectionError={addListener(){}};" +
          "this.$={bindNewPipeAndPassRemote:()=>({impl:impl}),bindHandle(){},close(){}};}}";
      }
      var fallback = entry.name === HANDLER_REMOTE ?
        "window.__fixedgeHost.method(String(p))" : "(()=>Promise.resolve({}))";
      return "class " + entry.name + "{constructor(){" +
        "const t={$:{bindNewPipeAndPassReceiver(){return{bindInBrowser(){},handle:null}},close(){}}," +
        "onConnectionError:{addListener(){}}};" +
        "return new Proxy(t,{get:(o,p)=>p in o?o[p]:" + fallback + "});}}";
    });
    return hostSource + "\n" + stubs.join("\n") + "\n" + renamed;
  }

  window.__fxg = {
    setLibrary: function (hostSource, library) {
      bridge();
      state.patched = patch(hostSource, library);
      return String(state.patched.length);
    },
    tryBoot: function () {
      if (!state.patched) {
        return "no-library";
      }
      if (root.dataset.fixedgeBooted === "1") {
        return "booted";
      }
      window.dispatchEvent(new MessageEvent("message", {
        data: { type: "glic-bootstrap", glicApiSource: state.patched },
        origin: "chrome://glic",
        source: window,
      }));
      return "sent";
    },
    drain: function () {
      var lines = ["fixedge-glic",
        "booted=" + (root.dataset.fixedgeBooted || ""),
        "client=" + (root.dataset.fixedgeClient || ""),
        "unavailable=" + (document.body && /isn.t available right now/.test(document.body.innerText) ? "1" : "0")];
      Array.from(bridge().querySelectorAll("i[data-id]")).forEach(function (request) {
        lines.push(["call", request.dataset.id, request.dataset.op, request.textContent].join("\t"));
        request.remove();
      });
      return lines.join("\n");
    },
    reply: function (id, ok, json) {
      var answer = document.createElement("b");
      answer.dataset.id = String(id);
      answer.textContent = JSON.stringify(ok ? { ok: true, value: JSON.parse(json) } : { ok: false, error: json });
      bridge().appendChild(answer);
      bridge().dispatchEvent(new Event("fixedge-reply"));
      return "0";
    },
    push: function (kind, json) {
      var update = document.createElement("u");
      update.dataset.kind = kind;
      update.textContent = json;
      bridge().appendChild(update);
      bridge().dispatchEvent(new Event("fixedge-reply"));
      return "0";
    },
  };
  return "ok";
})())JS";

}  // namespace fixedge::glic_host_script
