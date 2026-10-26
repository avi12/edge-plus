// find_scripts.h — the JavaScript the find bar runs inside the page. Matches are
// marked with the CSS Custom Highlight API, so the page's DOM is never changed;
// the scripts run in our isolated world, so the page's own scripts cannot see
// or answer them falsely. Answers are parsed by find_text.h / ask_text.h.
#pragma once
#include <span>
#include <string>
#include "assistant/ask_text.h"
#include "find/find_text.h"
#include "find/web_text.h"

namespace edgeplus::find_scripts {

// ask_text.h cuts a carried paragraph further for the prompt.
inline constexpr int kMaxReadChars = 60000;
inline constexpr int kMaxParagraphCarryChars = 1200;
inline constexpr int kMaxSearchChars = 4000000;

inline constexpr char kMatchHighlight[] = "edgeplus-find";
inline constexpr char kCurrentHighlight[] = "edgeplus-find-current";
inline constexpr char kMatchColor[] = "#ffe14d";
inline constexpr char kCurrentColor[] = "#ff8a1f";
inline constexpr char kPdfContentType[] = "application/pdf";

// Pixels kept clear above and below the current match when it scrolls into view.
inline constexpr int kScrollTopMargin = 110;
inline constexpr int kScrollBottomMargin = 40;

// Defined once per page. `collect` walks the visible text into one string (a
// newline between blocks, whitespace squeezed as the page shows it) with a map
// back to the text nodes, which `rangeFor` turns offsets into a Range through.
inline std::string Helpers() {
  std::string script = R"JS(
  var fx = window.__fxFind;
  if (!fx) {
    fx = window.__fxFind = { ranges: [], units: [], sheet: null, styleElement: null };
    var matchName = ")JS";
  script += kMatchHighlight;
  script += R"JS(";
    var currentName = ")JS";
  script += kCurrentHighlight;
  script += R"JS(";
    var skipTags = { SCRIPT: 1, STYLE: 1, NOSCRIPT: 1, TEMPLATE: 1 };
    var css = "::highlight(" + matchName + "){background-color:)JS";
  script += kMatchColor;
  script += R"JS(;color:#000}" +
              "::highlight(" + currentName + "){background-color:)JS";
  script += kCurrentColor;
  script += R"JS(;color:#000}";

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
      fx.units = [];
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
      var isClear = box.top >= )JS";
  script += std::to_string(kScrollTopMargin);
  script += R"JS( && box.bottom <= window.innerHeight - )JS";
  script += std::to_string(kScrollBottomMargin);
  script += R"JS(;
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

// Never throws out of the page: a failure answers the error line.
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
                               web_text::JsBoolField("isCapped"),
                               "String(current)"}) +
         ";";
}

inline std::string RequireHighlights() {
  return R"JS(
    if (!window.CSS || !CSS.highlights || typeof Highlight === "undefined") {
      return )JS" +
         web_text::JsErrorAnswer("This page cannot show highlights.") +
         R"JS(;
    })JS";
}

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

inline std::string ClearScript() {
  return R"JS((function () { if (window.__fxFind) { window.__fxFind.clear(); } return )JS" +
         web_text::JsOkAnswer() + "; })()";
}

// The page's paragraphs, each split into sentences; every sentence is
// remembered as a live Range so the ones Copilot names can be marked later.
inline std::string ReadParagraphsScript() {
  std::string body = R"JS(
    if (document.contentType === )JS";
  body += web_text::JsStringLiteral(kPdfContentType);
  body += R"JS() {
      return )JS";
  body += web_text::JsErrorAnswer("PDF pages cannot be read yet.");
  body += R"JS(;
    }
    var map = fx.collect();
    fx.units = [];
    var texts = [];
    var total = 0;
    var isCut = false;
    for (var i = 0; i < map.groups.length; i++) {
      var group = map.groups[i];
      var from = map.starts[group.from];
      var to = map.starts[group.to] + map.lens[group.to];
      var raw = map.text.slice(from, to);
      var text = raw.trim();
      if (!text) {
        continue;
      }
      var textStart = from + raw.search(/\S/);
      if (total + text.length > )JS";
  body += std::to_string(kMaxReadChars);
  body += R"JS() {
        isCut = true;
        break;
      }
      total += text.length;
      var sentences = [];
      var pattern = /[^.!?]+(?:[.!?]+["')\]]*(?=\s|$)|$)/g;
      var match;
      while ((match = pattern.exec(text)) !== null) {
        var sentence = match[0].trim();
        var begin = textStart + match.index + match[0].search(/\S/);
        var range = sentence ? fx.rangeFor(map, begin, begin + sentence.length) : null;
        if (range) {
          fx.units.push(range);
          sentences.push(sentence.slice(0, )JS";
  body += std::to_string(kMaxParagraphCarryChars);
  body += R"JS());
        }
      }
      if (sentences.length) {
        texts.push(sentences.join()JS";
  body +=
      web_text::JsStringLiteral(std::string(1, ask_text::kSentenceSeparator));
  body += R"JS());
      }
    }
    return )JS";
  const std::string paragraphSeparator =
      web_text::JsStringLiteral(std::string(1, ask_text::kParagraphSeparator));
  body += web_text::JsOkAnswer({web_text::JsBoolField("isCut"),
                                "texts.join(" + paragraphSeparator + ")"});
  body += ";";
  return Wrap(body);
}

// `indices` are positions in the sentence list ReadParagraphsScript answered; a
// sentence whose text has left the page is skipped.
inline std::string HighlightParagraphsScript(std::span<const int> indices) {
  std::string body = RequireHighlights();
  body += R"JS(
    var wanted = [)JS";
  body += ask_text::IndexList(indices);
  body += R"JS(];
    var ranges = [];
    wanted.forEach(function (index) {
      var unit = fx.units[index];
      if (unit && !unit.collapsed) {
        ranges.push(unit.cloneRange());
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

}  // namespace edgeplus::find_scripts
