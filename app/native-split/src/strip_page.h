// strip_page.h — the page and script of the Gemini panel's share strip. One
// concern: the markup, the style and the script that make the strip's two
// buttons ("Share this tab", "Pick tabs") and its tab picker. The strip is a
// WebView of our own at the top of the panel, loading this page as a data: URL;
// share.h polls it for clicks and pushes tabs and status into it. Fluent 2
// tokens by hand (like the panel app), light and dark following the browser.
#pragma once

namespace fixedge::strip_page {

// The strip page. It carries no script: the script below runs in our isolated
// world, so nothing in this document is reachable from a page of the web.
inline constexpr char kHtmlStyle[] = R"CSS(
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="color-scheme" content="light dark">
<title>Share with Gemini</title>
<style>
:root {
  --surface: #ffffff; --surface-hover: #f5f5f5; --surface-pressed: #e0e0e0;
  --stroke: #d1d1d1; --text: #242424; --text-muted: #616161;
  --brand: #0f6cbd; --brand-hover: #115ea3; --brand-pressed: #0f548c; --on-brand: #ffffff;
  --danger: #b10e1c; --success: #0e700e;
  --radius: 4px; --radius-lg: 8px;
  --font: "Segoe UI Variable Text", "Segoe UI", system-ui, sans-serif;
}
@media (prefers-color-scheme: dark) {
  :root {
    --surface: #292929; --surface-hover: #3d3d3d; --surface-pressed: #1f1f1f;
    --stroke: #666666; --text: #ffffff; --text-muted: #adadad;
    --brand: #479ef5; --brand-hover: #62abf5; --brand-pressed: #2886de; --on-brand: #000000;
    --danger: #dc626d; --success: #54b054;
  }
}
* { box-sizing: border-box; }
html, body { height: 100%; margin: 0; }
body {
  display: flex; flex-direction: column; overflow: hidden;
  background: var(--surface); color: var(--text);
  font: 400 13px/1.4 var(--font);
}
#bar {
  flex: none; height: 44px; display: flex; align-items: center; gap: 8px;
  padding: 0 10px; border-bottom: 1px solid var(--stroke);
}
button {
  font: inherit; color: var(--text); background: var(--surface);
  border: 1px solid var(--stroke); border-radius: var(--radius);
  height: 30px; padding: 0 10px; display: inline-flex; align-items: center; gap: 6px;
  cursor: pointer; white-space: nowrap;
}
button:hover { background: var(--surface-hover); }
button:active { background: var(--surface-pressed); }
button:focus-visible, input:focus-visible { outline: 2px solid var(--brand); outline-offset: 1px; }
button svg { width: 16px; height: 16px; fill: currentColor; flex: none; }
button.primary { background: var(--brand); border-color: transparent; color: var(--on-brand); font-weight: 600; }
button.primary:hover { background: var(--brand-hover); }
button.primary:active { background: var(--brand-pressed); }
button:disabled { opacity: .5; cursor: default; }
#status {
  flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  color: var(--text-muted); font-size: 12px;
}
#status[data-kind="error"] { color: var(--danger); }
#status[data-kind="ok"] { color: var(--success); }
#sheet { flex: 1; min-height: 0; display: none; flex-direction: column; }
body[data-sheet] #sheet { display: flex; }
#sheet-head { display: flex; gap: 8px; padding: 10px; border-bottom: 1px solid var(--stroke); }
#filter {
  flex: 1; min-width: 0; height: 30px; padding: 0 10px; font: inherit; color: var(--text);
  background: var(--surface); border: 1px solid var(--stroke); border-radius: var(--radius);
}
#list { flex: 1; min-height: 0; overflow-y: auto; margin: 0; padding: 6px; list-style: none; }
#list li { margin: 0; }
#list label {
  display: flex; align-items: center; gap: 10px; padding: 8px 10px;
  border-radius: var(--radius); cursor: pointer;
}
#list label:hover { background: var(--surface-hover); }
#list input { width: 16px; height: 16px; margin: 0; accent-color: var(--brand); flex: none; }
#list li[data-state="blocked"] label, #list li[data-state="asleep"] label { opacity: .55; cursor: default; }
.row-text { min-width: 0; flex: 1; display: flex; flex-direction: column; }
.row-title { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.row-meta { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--text-muted); font-size: 12px; }
.row-tag {
  flex: none; font-size: 11px; color: var(--text-muted);
  border: 1px solid var(--stroke); border-radius: 999px; padding: 0 8px;
}
#empty { padding: 20px; color: var(--text-muted); text-align: center; }
@media (prefers-reduced-motion: no-preference) {
  button { transition: background-color 80ms ease-out; }
}
</style>
)CSS";

inline constexpr char kHtmlBody[] = R"HTML(
<body>
<div id="bar">
  <button id="share" class="primary" type="button" title="Put this tab's page into the Gemini prompt">
    <svg viewBox="0 0 20 20" aria-hidden="true"><path d="M4.5 3A2.5 2.5 0 0 0 2 5.5v9A2.5 2.5 0 0 0 4.5 17h6.1a5.5 5.5 0 0 1-.58-1H4.5A1.5 1.5 0 0 1 3 14.5V7h14v2.02c.35.04.68.12 1 .24V5.5A2.5 2.5 0 0 0 15.5 3h-11ZM3 5.5C3 4.67 3.67 4 4.5 4h11c.83 0 1.5.67 1.5 1.5V6H3v-.5ZM15.5 10a4.5 4.5 0 1 0 0 9 4.5 4.5 0 0 0 0-9Zm.5 2.5V14h1.5a.5.5 0 0 1 0 1H16v1.5a.5.5 0 0 1-1 0V15h-1.5a.5.5 0 0 1 0-1H15v-1.5a.5.5 0 0 1 1 0Z"/></svg>
    Share this tab
  </button>
  <button id="pick" type="button" title="Choose other tabs to share" aria-expanded="false">
    <svg viewBox="0 0 20 20" aria-hidden="true"><path d="M3 5.5A2.5 2.5 0 0 1 5.5 3h9A2.5 2.5 0 0 1 17 5.5v9a2.5 2.5 0 0 1-2.5 2.5h-9A2.5 2.5 0 0 1 3 14.5v-9ZM5.5 4A1.5 1.5 0 0 0 4 5.5V7h12V5.5A1.5 1.5 0 0 0 14.5 4h-9ZM16 8H4v6.5A1.5 1.5 0 0 0 5.5 16h9a1.5 1.5 0 0 0 1.5-1.5V8Z"/></svg>
    Pick tabs
  </button>
  <div id="status" role="status" aria-live="polite">You decide when to send.</div>
</div>
<div id="sheet">
  <div id="sheet-head">
    <input id="filter" type="search" placeholder="Find a tab" aria-label="Find a tab" autocomplete="off">
    <button id="done" class="primary" type="button" disabled>Add to prompt</button>
    <button id="cancel" type="button">Cancel</button>
  </div>
  <ul id="list" aria-label="Open tabs"></ul>
</div>
</body>
</html>
)HTML";

// What the strip's script answers while the page is not ready, and what it
// answers once it has installed itself: ParsePoll (share_text.h) reads both.
inline constexpr char kLoadingAnswer[] = "loading";
inline constexpr char kInstalledAnswer[] = "ok";

// Installs the strip's behaviour in the isolated world; run until it answers
// kInstalledAnswer (the page may still be loading). Idempotent.
inline constexpr char kInstallScript[] = R"JS((function () {
  if (window.__fx) {
    return "ok";
  }
  var byId = function (id) { return document.getElementById(id); };
  if (document.readyState === "loading" || !byId("share") || !byId("list")) {
    return "loading";
  }
  var fx = window.__fx = { out: [], tabs: [], picked: {}, isOpen: false };

  function pickedIds() {
    return Object.keys(fx.picked).filter(function (id) { return fx.picked[id]; });
  }
  function setOpen(isOpen) {
    fx.isOpen = isOpen;
    document.body.toggleAttribute("data-sheet", isOpen);
    byId("pick").setAttribute("aria-expanded", isOpen ? "true" : "false");
    if (isOpen) {
      fx.picked = {};
      byId("filter").value = "";
      render();
      byId("filter").focus();
    }
  }
  function render() {
    var query = byId("filter").value.trim().toLowerCase();
    var list = byId("list");
    list.textContent = "";
    var shown = 0;
    fx.tabs.forEach(function (tab) {
      var haystack = (tab.title + " " + tab.host).toLowerCase();
      if (query && haystack.indexOf(query) < 0) {
        return;
      }
      shown++;
      var item = document.createElement("li");
      item.dataset.state = tab.state;
      var label = document.createElement("label");
      var box = document.createElement("input");
      box.type = "checkbox";
      box.disabled = tab.state !== "ready";
      box.checked = !!fx.picked[tab.id];
      box.addEventListener("change", function () {
        fx.picked[tab.id] = box.checked;
        refreshDone();
      });
      var text = document.createElement("span");
      text.className = "row-text";
      var title = document.createElement("span");
      title.className = "row-title";
      title.textContent = tab.title;
      var meta = document.createElement("span");
      meta.className = "row-meta";
      meta.textContent = tab.host;
      text.append(title, meta);
      label.append(box, text);
      var tagText = tab.state === "blocked" ? "Can't be shared"
        : tab.state === "asleep" ? "Asleep" : tab.current ? "Current" : "";
      if (tagText) {
        var tag = document.createElement("span");
        tag.className = "row-tag";
        tag.textContent = tagText;
        label.append(tag);
      }
      item.append(label);
      list.append(item);
    });
    if (!shown) {
      var empty = document.createElement("li");
      empty.id = "empty";
      empty.textContent = fx.tabs.length ? "No tab matches." : "Looking for your tabs...";
      list.append(empty);
    }
    refreshDone();
  }
  function refreshDone() {
    var count = pickedIds().length;
    byId("done").disabled = count === 0;
    byId("done").textContent = count > 1 ? "Add " + count + " tabs to prompt" : "Add to prompt";
  }

  byId("share").addEventListener("click", function () {
    setOpen(false);
    fx.out.push("share-active");
  });
  byId("pick").addEventListener("click", function () {
    if (fx.isOpen) {
      setOpen(false);
    } else {
      fx.tabs = [];
      setOpen(true);
      fx.out.push("list");
    }
  });
  byId("cancel").addEventListener("click", function () { setOpen(false); });
  byId("done").addEventListener("click", function () {
    var ids = pickedIds();
    setOpen(false);
    if (ids.length) {
      fx.out.push("share:" + ids.join(","));
    }
  });
  byId("filter").addEventListener("input", render);
  document.addEventListener("keydown", function (event) {
    if (event.key === "Escape" && fx.isOpen) {
      setOpen(false);
    }
  });

  fx.setTabs = function (tabs) {
    fx.tabs = tabs;
    if (fx.isOpen) {
      render();
    }
  };
  fx.status = function (kind, text) {
    var status = byId("status");
    status.dataset.kind = kind;
    status.textContent = text;
  };
  fx.drain = function () {
    var lines = ["fixedge-strip", "expanded=" + (fx.isOpen ? "1" : "0")].concat(fx.out);
    fx.out = [];
    return lines.join("\n");
  };
  return "ok";
})())JS";

// Asked every tick: the clicks since the last one (ParsePoll reads it), or
// "not-installed" if the script is missing.
inline constexpr char kPollScript[] =
    R"JS(window.__fx ? window.__fx.drain() : "not-installed")JS";

}  // namespace fixedge::strip_page
