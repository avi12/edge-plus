// find_page.h — the page and script of the find bar. One concern: the markup,
// the style and the script that make the bar's box, its three modes (text,
// regular expression, ask Copilot), its case and whole-word switches and its
// next / previous buttons. The bar is a WebView of our own over the top right of
// the page, loading this page as a data: URL; find.h polls it for what the user
// did and pushes the match count and status into it. The look is the shared
// Fluent 2 tokens (fluent_tokens.h).
#pragma once
#include <string>
#include "find_text.h"
#include "fluent_tokens.h"

namespace fixedge::find_page {

// What the install script answers while the page is not ready, and once it has
// installed itself.
inline constexpr char kInstalledAnswer[] = "ok";
inline constexpr char kLoadingAnswer[] = "loading";

inline constexpr char kFindCss[] = R"CSS(
body { display: flex; flex-direction: column; gap: 4px; padding: 6px 8px; border: 1px solid var(--stroke); }
.row { flex: none; height: 32px; display: flex; align-items: center; gap: 6px; }
#box {
  flex: 1; min-width: 0; height: 30px; padding: 0 10px; font: inherit; color: var(--text);
  background: var(--surface); border: 1px solid var(--stroke); border-radius: var(--radius);
}
#box:focus { border-color: var(--brand); }
body[data-invalid] #box { border-color: var(--danger); }
#count { flex: none; min-width: 64px; text-align: right; color: var(--text-muted); font-size: 12px; white-space: nowrap; }
.small { width: 30px; padding: 0; justify-content: center; }
#modes { display: flex; flex: none; }
#modes button { border-radius: 0; margin-left: -1px; height: 26px; padding: 0 9px; font-size: 12px; }
#modes button:first-child { border-radius: var(--radius) 0 0 var(--radius); margin-left: 0; }
#modes button:last-child { border-radius: 0 var(--radius) var(--radius) 0; }
#modes button[aria-pressed="true"] { background: var(--brand); border-color: var(--brand); color: var(--on-brand); }
.toggle { height: 26px; min-width: 30px; padding: 0 6px; justify-content: center; font-size: 12px; font-weight: 600; }
.toggle[aria-pressed="true"] { background: var(--surface-pressed); border-color: var(--brand); color: var(--brand); }
body[data-mode="ask"] .toggle { display: none; }
#status {
  flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  color: var(--text-muted); font-size: 12px;
}
#status[data-kind="error"] { color: var(--danger); }
#status[data-kind="ok"] { color: var(--success); }
body[data-busy] #box { opacity: .6; }
body[data-busy] #status::before {
  content: ""; display: inline-block; width: 9px; height: 9px; margin-right: 6px;
  border: 2px solid var(--stroke); border-top-color: var(--brand); border-radius: 50%;
}
@media (prefers-reduced-motion: no-preference) {
  body[data-busy] #status::before { animation: spin 800ms linear infinite; }
  @keyframes spin { to { transform: rotate(360deg); } }
}
)CSS";

inline constexpr char kHtmlBody[] = R"HTML(
<body data-mode="text">
<div class="row">
  <input id="box" type="text" autocomplete="off" spellcheck="false" aria-label="Find in page">
  <span id="count" role="status" aria-live="polite"></span>
  <button id="prev" class="small" type="button" title="Previous match (Shift+Enter)" aria-label="Previous match">
    <svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 6.2 16.2 12.4 15.1 13.5 10 8.4 4.9 13.5 3.8 12.4Z"/></svg>
  </button>
  <button id="next" class="small" type="button" title="Next match (Enter)" aria-label="Next match">
    <svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 13.8 16.2 7.6 15.1 6.5 10 11.6 4.9 6.5 3.8 7.6Z"/></svg>
  </button>
  <button id="close" class="small" type="button" title="Close (Esc)" aria-label="Close">
    <svg viewBox="0 0 20 20" aria-hidden="true"><path d="M5 4 10 9 15 4 16 5 11 10 16 15 15 16 10 11 5 16 4 15 9 10 4 5Z"/></svg>
  </button>
</div>
<div class="row">
  <div id="modes" role="group" aria-label="Search mode">
    <button type="button" data-mode="text" aria-pressed="true" title="Find the words you type">Text</button>
    <button type="button" data-mode="regex" aria-pressed="false" title="Find a regular expression">Regex</button>
    <button type="button" data-mode="ask" aria-pressed="false" title="Ask Copilot where the page covers something">Ask Copilot</button>
  </div>
  <button id="case" class="toggle" type="button" aria-pressed="false" title="Match case">Aa</button>
  <button id="word" class="toggle" type="button" aria-pressed="false" title="Whole word">ab</button>
  <div id="status" role="status" aria-live="polite"></div>
</div>
</body>
</html>
)HTML";

// The page, whole.
inline std::string Html() {
  return std::string(fluent_tokens::kPageHead) +
         "<title>Find in page</title>\n<style>" + fluent_tokens::kBaseCss +
         kFindCss + "</style>\n" + kHtmlBody;
}

// Installs the bar's behaviour in the isolated world; run until it answers
// kInstalledAnswer (the page may still be loading). Idempotent. What it reports
// is find_text::ParseReport's format: a line per thing the user did.
inline std::string InstallScript() {
  std::string script = R"JS((function () {
  if (window.__ff) {
    return ")JS";
  script += kInstalledAnswer;
  script += R"JS(";
  }
  var byId = function (id) { return document.getElementById(id); };
  if (document.readyState === "loading" || !byId("box") || !byId("close")) {
    return ")JS";
  script += kLoadingAnswer;
  script += R"JS(";
  }
  var US = "\x1f";
  var placeholders = {
    text: "Find in page",
    regex: "Regular expression, such as \\d{3}-\\d{4}",
    ask: "Ask Copilot about this page"
  };
  var ff = window.__ff = { out: [], mode: "text", isCase: false, isWord: false };

  function pushBox(kind) {
    var text = byId("box").value;
    ff.out.push([kind, ff.mode, ff.isCase ? "1" : "0", ff.isWord ? "1" : "0", text].join(US));
  }
  function pushQuery() {
    pushBox("query");
  }
  function setMode(mode) {
    ff.mode = mode;
    document.body.dataset.mode = mode;
    Array.from(document.querySelectorAll("#modes button")).forEach(function (button) {
      button.setAttribute("aria-pressed", button.dataset.mode === mode ? "true" : "false");
    });
    byId("box").placeholder = placeholders[mode];
    ff.setCount("");
    ff.setStatus("info", "");
    pushQuery();
    byId("box").focus();
  }
  function setToggle(id, isOn) {
    byId(id).setAttribute("aria-pressed", isOn ? "true" : "false");
  }

  Array.from(document.querySelectorAll("#modes button")).forEach(function (button) {
    button.addEventListener("click", function () { setMode(button.dataset.mode); });
  });
  byId("case").addEventListener("click", function () {
    ff.isCase = !ff.isCase;
    setToggle("case", ff.isCase);
    pushQuery();
  });
  byId("word").addEventListener("click", function () {
    ff.isWord = !ff.isWord;
    setToggle("word", ff.isWord);
    pushQuery();
  });
  byId("next").addEventListener("click", function () { ff.out.push("next"); });
  byId("prev").addEventListener("click", function () { ff.out.push("prev"); });
  byId("close").addEventListener("click", function () { ff.out.push("close"); });
  byId("box").addEventListener("input", function () {
    if (ff.mode !== "ask") {
      pushQuery();
    }
  });
  document.addEventListener("keydown", function (event) {
    var isFind = (event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "f";
    if (event.key === "Escape") {
      event.preventDefault();
      ff.out.push("close");
    } else if (isFind) {
      event.preventDefault();
      byId("box").focus();
      byId("box").select();
    } else if (event.key === "Enter" && event.target === byId("box")) {
      event.preventDefault();
      if (ff.mode === "ask") {
        if (byId("box").value.trim()) {
          pushBox("ask");
        }
      } else {
        ff.out.push(event.shiftKey ? "prev" : "next");
      }
    }
  });
  // Start (or restart) a search session: `text`, when given, replaces what is
  // in the box; the box takes the focus with its text selected.
  ff.open = function (text) {
    if (text) {
      byId("box").value = text;
    }
    byId("box").focus();
    byId("box").select();
    if (byId("box").value && ff.mode !== "ask") {
      pushQuery();
    }
  };
  ff.setCount = function (label) {
    byId("count").textContent = label;
  };
  ff.setStatus = function (kind, text) {
    var status = byId("status");
    status.dataset.kind = kind;
    status.textContent = text;
    status.title = text;
    document.body.toggleAttribute("data-invalid", kind === "error");
  };
  ff.setBusy = function (isBusy) {
    document.body.toggleAttribute("data-busy", isBusy);
    byId("box").readOnly = isBusy;
  };
  ff.drain = function () {
    var lines = [")JS";
  script += find_text::kBarMarker;
  script += R"JS("].concat(ff.out);
    ff.out = [];
    return lines.join("\n");
  };
  byId("box").value = "";
  setMode("text");
  ff.out = [];
  return ")JS";
  script += kInstalledAnswer;
  script += R"JS(";
})())JS";
  return script;
}

// Asked every tick: what the user did since the last one (find_text::
// ParseReport reads it), or kNotInstalledAnswer if the script is missing.
inline std::string PollScript() {
  return std::string("window.__ff ? window.__ff.drain() : \"") +
         find_text::kNotInstalledAnswer + "\"";
}

}  // namespace fixedge::find_page
