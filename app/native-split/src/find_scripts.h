// find_scripts.h — the scripts the find bar runs inside the page being searched.
// One concern: the JavaScript that marks matches (with the CSS Custom Highlight
// API, so the page's own DOM is never changed), moves between them, reads the
// page's paragraphs for Ask Copilot and marks the ones Copilot named. They run
// in our isolated world, so the page's own scripts cannot see or answer them
// falsely. Every answer is a plain string that find_text.h / ask_text.h parse.
#pragma once
#include <string>
#include "ask_text.h"
#include "find_text.h"
#include "web_text.h"

namespace fixedge::find_scripts {

// How much of a page is read for Ask Copilot, and how much of one paragraph is
// carried out of the page (ask_text.h cuts the rest for the prompt).
inline constexpr int kMaxReadChars = 60000;
inline constexpr int kMaxParagraphCarryChars = 1200;
// How much of a page a search looks through.
inline constexpr int kMaxSearchChars = 4000000;

// The highlight names the page's stylesheet is written for.
inline constexpr char kMatchHighlight[] = "fixedge-find";
inline constexpr char kCurrentHighlight[] = "fixedge-find-current";

// The page-side helpers, defined once per page (idempotent): `collect` walks
// the page's visible text into one string (a newline between blocks, runs of
// whitespace squeezed as the page shows them) with a map back to the text
// nodes; `rangeFor` turns an offset pair in that string into a DOM Range;
// `show`, `goTo` and `clear` own the highlights.
inline std::string Helpers() {
  std::string script = R"JS(
  var fx = window.__fxFind;
  if (!fx) {
    fx = window.__fxFind = { ranges: [], paragraphs: [], sheet: null, styleElement: null };
    var matchName = ")JS";
  script += kMatchHighlight;
  script += R"JS(";
    var currentName = ")JS";
  script += kCurrentHighlight;
  script += R"JS(";
    var skipTags = { SCRIPT: 1, STYLE: 1, NOSCRIPT: 1, TEMPLATE: 1 };
    var css = "::highlight(" + matchName + "){background-color:#ffe14d;color:#000}" +
              "::highlight(" + currentName + "){background-color:#ff8a1f;color:#000}";

    fx.ensureStyle = function () {
      if (fx.sheet || fx.styleElement) {
        return;
      }
      try {
        var sheet = new CSSStyleSheet();
        sheet.replaceSync(css);
        document.adoptedStyleSheets = document.adoptedStyleSheets.concat([sheet]);
        fx.sheet = sheet;
      } catch (error) {
        var element = document.createElement("style");
        element.textContent = css;
        (document.head || document.documentElement).appendChild(element);
        fx.styleElement = element;
      }
    };

    fx.clear = function () {
      if (window.CSS && CSS.highlights) {
        CSS.highlights.delete(matchName);
        CSS.highlights.delete(currentName);
      }
      if (fx.sheet) {
        var kept = Array.from(document.adoptedStyleSheets).filter(function (each) { return each !== fx.sheet; });
        document.adoptedStyleSheets = kept;
        fx.sheet = null;
      }
      if (fx.styleElement) {
        fx.styleElement.remove();
        fx.styleElement = null;
      }
      fx.ranges = [];
      fx.paragraphs = [];
    };

    fx.show = function (ranges) {
      fx.ranges = ranges;
      fx.ensureStyle();
      var all = new Highlight();
      ranges.forEach(function (range) { all.add(range); });
      CSS.highlights.set(matchName, all);
      CSS.highlights.delete(currentName);
    };

    fx.goTo = function (index) {
      var range = fx.ranges[index];
      if (!range) {
        return;
      }
      var current = new Highlight(range);
      current.priority = 1;
      CSS.highlights.set(currentName, current);
      var box = range.getBoundingClientRect();
      var margin = 110;
      var isClear = box.top >= margin && box.bottom <= window.innerHeight - 40;
      if (!isClear) {
        var node = range.startContainer;
        var element = node.nodeType === 1 ? node : node.parentElement;
        if (element) {
          element.scrollIntoView({ block: "center", inline: "nearest", behavior: "instant" });
        }
      }
    };

    // The first match on or below the top of the window, so typing does not
    // throw the page back to the top.
    fx.firstInView = function () {
      for (var i = 0; i < fx.ranges.length; i++) {
        if (fx.ranges[i].getBoundingClientRect().bottom >= 0) {
          return i;
        }
      }
      return 0;
    };

    fx.collect = function () {
      var root = document.body || document.documentElement;
      var walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
      var info = new Map();
      var infoOf = function (element) {
        var known = info.get(element);
        if (known) {
          return known;
        }
        var isVisible = !element.closest("script,style,noscript,template") &&
          (!element.checkVisibility || element.checkVisibility({ visibilityProperty: true }));
        var style = isVisible ? getComputedStyle(element) : null;
        var display = style ? style.display : "none";
        var isInline = display.indexOf("inline") === 0 || display === "contents";
        var created = {
          isVisible: isVisible,
          isPre: style ? style.whiteSpace.indexOf("pre") === 0 : false,
          block: null
        };
        if (isVisible) {
          created.block = isInline && element.parentElement ? infoOf(element.parentElement).block : element;
        }
        info.set(element, created);
        return created;
      };
      var map = { text: "", nodes: [], starts: [], lens: [], origin: [], groups: [] };
      var lastBlock = null;
      var node;
      while ((node = walker.nextNode())) {
        var data = node.nodeValue;
        var parent = node.parentElement;
        if (!data || !parent) {
          continue;
        }
        var details = infoOf(parent);
        if (!details.isVisible) {
          continue;
        }
        var piece = data;
        var origin = null;
        if (!details.isPre && / {2}|[\n\t\r\f]/.test(data)) {
          piece = "";
          origin = [];
          for (var i = 0; i < data.length; i++) {
            var isSpace = " \t\n\r\f".indexOf(data[i]) >= 0;
            var isRun = isSpace && piece.length > 0 && piece[piece.length - 1] === " " && origin.length > 0 &&
                        origin[origin.length - 1].isSpace;
            if (isRun) {
              origin[origin.length - 1].end = i + 1;
            } else {
              piece += isSpace ? " " : data[i];
              origin.push({ start: i, end: i + 1, isSpace: isSpace });
            }
          }
        }
        if (map.nodes.length && details.block !== lastBlock) {
          map.text += "\n";
          map.groups.push({ from: map.nodes.length, to: map.nodes.length });
        } else if (!map.nodes.length) {
          map.groups.push({ from: 0, to: 0 });
        }
        lastBlock = details.block;
        map.groups[map.groups.length - 1].to = map.nodes.length;
        map.nodes.push(node);
        map.starts.push(map.text.length);
        map.lens.push(piece.length);
        map.origin.push(origin);
        map.text += piece;
        if (map.text.length > )JS";
  script += std::to_string(kMaxSearchChars);
  script += R"JS() {
          break;
        }
      }
      return map;
    };

    // The DOM position of offset `offset` of the collected text.
    fx.locate = function (map, offset, isEnd) {
      var low = 0;
      var high = map.starts.length - 1;
      while (low < high) {
        var middle = (low + high + 1) >> 1;
        if (map.starts[middle] <= offset) {
          low = middle;
        } else {
          high = middle - 1;
        }
      }
      var index = low;
      var local = offset - map.starts[index];
      var length = map.lens[index];
      if (isEnd) {
        local = Math.min(local, length);
      } else if (local >= length) {
        index++;
        local = 0;
        if (index >= map.nodes.length) {
          return null;
        }
      }
      var origin = map.origin[index];
      var position = local;
      if (origin) {
        if (isEnd) {
          position = local === 0 ? 0 : origin[local - 1].end;
        } else {
          position = origin[local].start;
        }
      }
      return { node: map.nodes[index], offset: position };
    };

    fx.rangeFor = function (map, start, end) {
      var from = fx.locate(map, start, false);
      var to = fx.locate(map, end, true);
      if (!from || !to) {
        return null;
      }
      var range = document.createRange();
      range.setStart(from.node, from.offset);
      range.setEnd(to.node, to.offset);
      return range;
    };
  }
)JS";
  return script;
}

// `body` (a script's own statements, using `fx`) wrapped so it runs with the
// helpers defined and never throws out of the page: a failure answers the
// error line.
inline std::string Wrap(const std::string& body) {
  return "(function () {\n  try {" + Helpers() + body +
         R"JS(
  } catch (error) {
    return )JS" +
         web_text::JsErrorAnswer("The page would not let Edge search it.") +
         R"JS(;
  }
})())JS";
}

// `ok US count US isCapped US current`, once `fx.ranges` holds the matches.
inline std::string ShownAnswer() {
  return "return " +
         web_text::JsOkAnswer({"String(fx.ranges.length)",
                               R"JS(isCapped ? "1" : "0")JS",
                               "String(current)"}) +
         ";";
}

// The page-side check that the CSS Custom Highlight API is there.
inline std::string RequireHighlights() {
  return R"JS(
    if (!window.CSS || !CSS.highlights || typeof Highlight === "undefined") {
      return )JS" +
         web_text::JsErrorAnswer("This page cannot show highlights.") +
         R"JS(;
    })JS";
}

// Mark every match of `source` (a RegExp source) with `flags`, scroll to the
// first one in view, and answer how many there are. Answers `err` with the
// RegExp's own message when it will not compile.
inline std::string SearchScript(const find_text::Search& search) {
  std::string body = RequireHighlights();
  body += R"JS(
    fx.clear();
    var regex;
    try {
      regex = new RegExp()JS";
  body += web_text::JsStringLiteral(search.source) + ", " +
          web_text::JsStringLiteral(search.flags);
  body += R"JS();
    } catch (error) {
      return )JS";
  body += web_text::JsErrorAnswerFrom(
      R"JS(String(error.message).replace(/^[\s\S]*\/[a-z]*: /, ""))JS");
  body += R"JS(;
    }
    var map = fx.collect();
    var ranges = [];
    var isCapped = false;
    var found;
    // g: every match, else only the first. y: the first match is found anywhere,
    // then each next one must begin where the one before it ended. The scan runs
    // as a global, non-sticky copy until the first match, so a missing g or y in
    // the typed flags never makes exec() stand still.
    var isEveryMatch = regex.global;
    var isSticky = regex.sticky;
    var scan = new RegExp(regex.source, regex.flags.replace("y", "").replace("g", "") + "g");
    while ((found = scan.exec(map.text)) !== null) {
      if (found[0].length === 0) {
        if (scan === regex) {
          break;
        }
        scan.lastIndex++;
        continue;
      }
      var range = fx.rangeFor(map, found.index, found.index + found[0].length);
      if (range) {
        ranges.push(range);
      }
      if (!isEveryMatch) {
        break;
      }
      if (isSticky && scan !== regex) {
        regex.lastIndex = found.index + found[0].length;
        scan = regex;
      }
      if (ranges.length >= )JS";
  body += std::to_string(find_text::kMaxMatches);
  body += R"JS() {
        isCapped = true;
        break;
      }
    }
    fx.show(ranges);
    var current = ranges.length ? fx.firstInView() : 0;
    if (ranges.length) {
      fx.goTo(current);
    }
    )JS";
  body += ShownAnswer();
  return Wrap(body);
}

// Make match `index` the current one and scroll it into view. Answers `ok`, or
// `err` when the page no longer holds the matches (it was reloaded).
inline std::string GoToScript(int index) {
  return std::string(R"JS((function () {
  var fx = window.__fxFind;
  if (!fx || !fx.ranges[)JS") +
         std::to_string(index) + R"JS(]) {
    return )JS" +
         web_text::JsErrorAnswer("The page changed. Search again.") + R"JS(;
  }
  fx.goTo()JS" +
         std::to_string(index) + R"JS();
  return )JS" +
         web_text::JsOkAnswer() + R"JS(;
})())JS";
}

// Take every highlight and the stylesheet back out of the page.
inline std::string ClearScript() {
  return R"JS((function () { if (window.__fxFind) { window.__fxFind.clear(); } return )JS" +
         web_text::JsOkAnswer() + "; })()";
}

// Read the page as paragraphs for Ask Copilot: one per block of text, in
// reading order, remembered so ones Copilot names can be marked later. Answers
// ask_text::ParsePageParagraphs' format. A PDF has no text in the page (Edge's
// viewer draws it), so it is refused.
inline std::string ReadParagraphsScript() {
  std::string body = R"JS(
    if (document.contentType === "application/pdf") {
      return )JS";
  body += web_text::JsErrorAnswer("PDF pages cannot be read yet.");
  body += R"JS(;
    }
    var map = fx.collect();
    fx.paragraphs = [];
    var texts = [];
    var total = 0;
    var isCut = false;
    for (var i = 0; i < map.groups.length; i++) {
      var group = map.groups[i];
      var from = map.starts[group.from];
      var to = map.starts[group.to] + map.lens[group.to];
      var text = map.text.slice(from, to).trim();
      if (!text) {
        continue;
      }
      if (total + text.length > )JS";
  body += std::to_string(kMaxReadChars);
  body += R"JS() {
        isCut = true;
        break;
      }
      total += text.length;
      fx.paragraphs.push({ first: map.nodes[group.from], last: map.nodes[group.to] });
      texts.push(text.slice(0, )JS";
  body += std::to_string(kMaxParagraphCarryChars);
  body += R"JS());
    }
    return )JS";
  const std::string paragraphSeparator =
      web_text::JsStringLiteral(std::string(1, ask_text::kParagraphSeparator));
  body += web_text::JsOkAnswer(
      {R"JS(isCut ? "1" : "0")JS", "texts.join(" + paragraphSeparator + ")"});
  body += ";";
  return Wrap(body);
}

// Mark the paragraphs `indices` (positions in the list ReadParagraphsScript
// answered) and scroll to the first. Answers find_text::ParseSearchResult's
// format; a paragraph whose text has left the page is skipped.
inline std::string HighlightParagraphsScript(const std::vector<int>& indices) {
  std::string body = RequireHighlights();
  body += R"JS(
    var wanted = [)JS";
  body += ask_text::IndexList(indices);
  body += R"JS(];
    var paragraphs = fx.paragraphs;
    var ranges = [];
    wanted.forEach(function (index) {
      var paragraph = paragraphs[index];
      if (paragraph && paragraph.first.isConnected && paragraph.last.isConnected) {
        var range = document.createRange();
        range.setStart(paragraph.first, 0);
        range.setEnd(paragraph.last, paragraph.last.nodeValue.length);
        ranges.push(range);
      }
    });
    fx.show(ranges);
    var isCapped = false;
    var current = 0;
    if (ranges.length) {
      fx.goTo(0);
    }
    )JS";
  body += ShownAnswer();
  return Wrap(body);
}

}  // namespace fixedge::find_scripts
