// page_scripts.h — the scripts run inside pages for tab sharing. One concern:
// the JavaScript that reads a tab (run in the tab being shared) and the
// JavaScript that puts text into Gemini's prompt box (run in the Gemini page).
// Both answer with a plain string that share_text.h parses or share.h checks;
// both run in our isolated world, so the page's own scripts cannot see them.
#pragma once
#include <string>
#include "share_text.h"

namespace fixedge::page_scripts {

// What the Gemini script answers.
inline constexpr char kInsertedAnswer[] = "ok";
inline constexpr char kNoPromptBoxAnswer[] = "no-prompt-box";

// Read the tab: its address, title, selection and visible text (cut at
// kMaxPageChars). The answer is share_text::ParseSnapshot's format. A PDF has
// no text in the page (Edge's viewer draws it), so it is refused rather than
// shared empty.
inline std::string ReadTabScript() {
  std::string script = R"JS((function () {
  var separator = "\x1f";
  var clean = function (text) { return String(text).split(separator).join(" "); };
  try {
    if (document.contentType === "application/pdf") {
      return "err" + separator + "PDF pages cannot be read yet.";
    }
    var limit = )JS";
  script += std::to_string(share_text::kMaxPageChars);
  script += R"JS(;
    var body = document.body;
    var text = body ? body.innerText : "";
    var isCut = text.length > limit;
    if (isCut) {
      text = text.slice(0, limit);
    }
    var selection = String(window.getSelection ? window.getSelection() : "");
    if (selection.length > limit) {
      selection = selection.slice(0, limit);
    }
    return ["ok", clean(location.href), clean(document.title), isCut ? "1" : "0",
            clean(selection), clean(text)].join(separator);
  } catch (error) {
    return "err" + separator + "The page would not let Edge read it.";
  }
})())JS";
  return script;
}

// Put `text` at the end of Gemini's prompt box and leave the caret after it,
// without sending anything. The box is a rich editor, so the text goes in
// through the editing command a paste would use.
inline std::string InsertIntoPromptScript(const std::string& text) {
  std::string script = R"JS((function (text) {
  var box = document.querySelector('rich-textarea .ql-editor[contenteditable="true"]') ||
            document.querySelector('.ql-editor[contenteditable="true"]') ||
            document.querySelector('[role="textbox"][contenteditable="true"]') ||
            document.querySelector("textarea");
  if (!box) {
    return "no-prompt-box";
  }
  box.focus();
  if (box.isContentEditable) {
    var selection = window.getSelection();
    var range = document.createRange();
    range.selectNodeContents(box);
    range.collapse(false);
    selection.removeAllRanges();
    selection.addRange(range);
    document.execCommand("insertText", false, text);
    box.scrollTop = box.scrollHeight;
  } else {
    box.setRangeText(text, box.value.length, box.value.length, "end");
    box.dispatchEvent(new Event("input", { bubbles: true }));
  }
  return "ok";
}))JS";
  // The call's argument: the whole text as one string literal.
  script += "(" + share_text::JsStringLiteral(text) + ")";
  return script;
}

}  // namespace fixedge::page_scripts
