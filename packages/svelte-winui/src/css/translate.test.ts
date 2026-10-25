import { computeDeclarations, matchingRules, resolveBuiltVariable, styleXaml } from "./cascade.ts";
import { loadWinuiCatalogue } from "./catalogue.ts";
import { lowerDeclarations } from "./properties.ts";
import { parseSelectorList } from "./selectors.ts";
import { errorsOf, formatDiagnostic, translateStyleSheet } from "./translate.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

const resources = loadWinuiCatalogue();

function translate(css: string, map?: { mappings: string }) {
  return translateStyleSheet({
    file: "card.svelte",
    css,
    map,
    resources
  });
}

function errors(css: string): string[] {
  return errorsOf(translate(css)).map(formatDiagnostic);
}

test("selectors: classes, a type, a state and Svelte's scope", () => {
  const [result] = parseSelectorList("text-block.title.svelte-abc:hover");
  assert.deepEqual(result, {
    isValid: true,
    selector: {
      elementType: "text-block",
      classes: ["title", "svelte-abc"],
      state: "hover",
      specificity: 4,
      isRoot: false
    }
  });
});

test("selectors: :where() adds its classes at no specificity", () => {
  const [result] = parseSelectorList(".card:where(.svelte-abc)");
  assert.equal(result?.isValid && result.selector.specificity, 1);
  assert.deepEqual(result?.isValid && result.selector.classes, ["card", "svelte-abc"]);
});

test("selectors: a list is several selectors", () => {
  assert.equal(parseSelectorList(".a, .b").length, 2);
});

test("selectors: every state WinUI reports", () => {
  for (const state of ["hover", "active", "focus", "focus-visible", "disabled", "checked"]) {
    const [result] = parseSelectorList(`.a:${state}`);
    assert.equal(result?.isValid && result.selector.state, state);
  }
});

test("selectors: a combinator says why it is not supported", () => {
  for (const selector of [".a .b", ".a > .b", ".a + .b", ".a ~ .b"]) {
    const [result] = parseSelectorList(selector);
    assert.equal(result?.isValid, false);
    assert.match(result?.isValid === false ? result.reason : "", /descendant, child and sibling selectors have no WinUI meaning/u);
  }
});

test("selectors: attributes, ids, other pseudo-classes and two states are rejected", () => {
  assert.equal(parseSelectorList("[a]")[0]?.isValid, false);
  assert.equal(parseSelectorList("#a")[0]?.isValid, false);
  assert.equal(parseSelectorList(".a:first-child")[0]?.isValid, false);
  assert.equal(parseSelectorList(".a::before")[0]?.isValid, false);
  assert.equal(parseSelectorList(".a:hover:focus")[0]?.isValid, false);
});

test("a sheet becomes rules in source order, one per selector", () => {
  const { sheet, diagnostics } = translate(".card.svelte-x { padding: 12px 16px }\n.a, text-block.b { opacity: .5 }");
  assert.deepEqual(diagnostics, []);
  assert.equal(sheet.id, "card.svelte");
  assert.deepEqual(sheet.rules.map(rule => [rule.elementType, rule.classes, rule.order]), [
    [null, ["card", "svelte-x"], 0],
    [null, ["a"], 1],
    ["text-block", ["b"], 2]
  ]);
});

test("a rule's shorthands are stored as longhands", () => {
  const { sheet } = translate(".a { padding: 1px 2px; border: 1px solid red }");
  assert.deepEqual(sheet.rules[0]?.declarations.map(declaration => declaration.property), [
    "padding-top",
    "padding-right",
    "padding-bottom",
    "padding-left",
    "border-top-width",
    "border-right-width",
    "border-bottom-width",
    "border-left-width",
    "border-style",
    "border-color"
  ]);
});

test("custom properties on :root become a dictionary with light and dark", () => {
  const { sheet, diagnostics } = translate(`
:root { --accent: #0af; --gap: 8px }
@media (prefers-color-scheme: dark) { :root { --accent: #fa0 } }
.a { color: var(--accent) }`);
  assert.deepEqual(diagnostics, []);
  assert.deepEqual(sheet.rules.length, 1);
  const xaml = sheet.resourcesXaml ?? "";
  assert.match(xaml, /<ResourceDictionary x:Key="Default">/u);
  assert.match(xaml, /<ResourceDictionary x:Key="Dark">/u);
  assert.match(xaml, /<Color x:Key="Accent">#00AAFF<\/Color>/u);
  assert.match(xaml, /<SolidColorBrush x:Key="AccentBrush" Color="\{StaticResource Accent\}"\/>/u);
  assert.match(xaml, /<Color x:Key="Accent">#FFAA00<\/Color>/u);
  assert.match(xaml, /<x:Double x:Key="Gap">8<\/x:Double>/u);
});

test("a custom property declared on a class is hoisted to the resources", () => {
  const { sheet, diagnostics } = translate(".a { --tone: red; color: var(--tone) }");
  assert.deepEqual(diagnostics, []);
  assert.match(sheet.resourcesXaml ?? "", /<Color x:Key="Tone">#FF0000<\/Color>/u);
  assert.deepEqual(sheet.rules[0]?.declarations.map(declaration => declaration.property), ["color"]);
});

test("a custom property declared for dark only is an error", () => {
  const result = errors("@media (prefers-color-scheme: dark) { :root { --x: red } }");
  assert.equal(result.length, 1);
  assert.match(result[0] ?? "", /--x is declared for dark only/u);
});

test("a custom property must be a color or a length", () => {
  assert.match(errors(":root { --x: bold }").join(), /--x: expected a color or a length in px, found "bold"/u);
});

test("var(): a WinUI name is checked, a typo gets a suggestion", () => {
  const result = errors(".a { background: var(--card-background-fill-colour-default) }");
  assert.equal(result.length, 1);
  assert.match(result[0] ?? "", /^card\.svelte:1:6: background-color: --card-background-fill-colour-default is not a WinUI resource/u);
  assert.match(result[0] ?? "", /did you mean --card-background-fill-color-default/u);
});

test("var(): a color used where a brush is needed is explained", () => {
  assert.match(errors(".a { padding: var(--control-corner-radius) }").join(), /--control-corner-radius is a WinUI CornerRadius resource \(ControlCornerRadius\), and a Thickness is needed here/u);
});

test("var(): brushes and thicknesses and radii resolve to ThemeResource", () => {
  const { sheet, diagnostics } = translate(".a { padding: var(--button-padding); border-radius: var(--control-corner-radius); color: var(--text-fill-color-primary) }");
  assert.deepEqual(diagnostics, []);
  assert.equal(sheet.rules.length, 1);
});

test("var(): a custom property of another sheet is unknown here", () => {
  assert.match(errors(".a { color: var(--elsewhere) }").join(), /--elsewhere is not a WinUI resource, nor a custom property this stylesheet declares/u);
});

test("what a sheet cannot say is an error with its position", () => {
  const result = errors(".a { color: red }\n.b {\n  box-shadow: 0 0 2px red;\n}");
  assert.deepEqual(result, ["card.svelte:3:3: box-shadow has no WinUI meaning (a WinUI shadow is a ThemeShadow set on the element, not a Style value)"]);
});

test("positions are mapped back to the .svelte file", () => {
  // Generated line 1 maps to line 10, column 0 (AAUA: +0,+0,+10,+0 is "AAUA" with U=10).
  const result = translate("box-shadow: x;\n.a { box-shadow: y }", { mappings: "AAUA;AAAA" });
  const messages = result.diagnostics.map(formatDiagnostic);
  assert.match(messages.join("\n"), /card\.svelte:11:/u);
});

test("descendant selectors, nesting, @keyframes and other @media are errors", () => {
  assert.match(errors(".a .b { color: red }").join(), /descendant, child and sibling selectors have no WinUI meaning/u);
  assert.match(errors(".a { color: red; &:hover { color: blue } }").join(), /nested rules have no WinUI meaning/u);
  assert.match(errors("@keyframes spin { to { opacity: 0 } }").join(), /@keyframes has no WinUI meaning/u);
  assert.match(errors("@media (min-width: 600px) { .a { color: red } }").join(), /@media \(min-width: 600px\) has no WinUI meaning/u);
  assert.match(errors("@import 'a.css';").join(), /@import has no WinUI meaning/u);
});

test("a style rule cannot change with the color scheme", () => {
  assert.match(errors("@media (prefers-color-scheme: dark) { .a { color: red } }").join(), /color has no WinUI meaning here \(a Style cannot depend on the color scheme/u);
});

test(":root cannot style an element", () => {
  assert.match(errors(":root { color: red }").join(), /color has no WinUI meaning \(:root styles no element/u);
});

test("a syntax error is reported, not thrown", () => {
  assert.equal(errors(".a { color: red }\n}").length, 1);
});

// The cascade.

function stylesFor(css: string, facts: { elementType: string; classes: string[]; states?: string[] }, inline = "") {
  const { sheet } = translate(css);
  const ranked = sheet.rules.map(rule => ({
    rule,
    id: String(rule.order),
    rank: rule.order
  }));
  const matched = matchingRules(ranked, {
    elementType: facts.elementType,
    classes: new Set(facts.classes),
    states: new Set(facts.states?.flatMap(state => (state === "hover" ? ["hover" as const] : [])) ?? [])
  });
  const inlineRule = translate(`.i { ${inline} }`).sheet.rules[0]?.declarations ?? [];
  const lowered = lowerDeclarations(computeDeclarations(matched, inlineRule), {
    elementType: facts.elementType,
    resolveVariable: resolveBuiltVariable,
    hasStyle: () => true,
    report: () => {}
  });
  return lowered.setters.map(entry => [entry.property, entry.kind === "setter" ? entry.value : entry.xaml]);
}

test("cascade: every class must be on the element; a type-only rule matches the type", () => {
  const css = ".a.b { opacity: 0.1 }\ntext-block { opacity: 0.2 }";
  assert.deepEqual(stylesFor(css, {
    elementType: "border",
    classes: ["a"]
  }), []);
  assert.deepEqual(stylesFor(css, {
    elementType: "border",
    classes: ["a", "b"]
  }), [["Opacity", "0.1"]]);
  assert.deepEqual(stylesFor(css, {
    elementType: "text-block",
    classes: []
  }), [["Opacity", "0.2"]]);
});

test("cascade: higher specificity wins whatever the order, then the later rule", () => {
  const css = ".a.b { opacity: 0.1 }\n.a { opacity: 0.2 }\n.b { opacity: 0.3 }";
  assert.deepEqual(stylesFor(css, {
    elementType: "border",
    classes: ["a", "b"]
  }), [["Opacity", "0.1"]]);
  assert.deepEqual(stylesFor(css, {
    elementType: "border",
    classes: ["a"]
  }), [["Opacity", "0.2"]]);
});

test("cascade: a state rule applies only while its state is active", () => {
  const css = ".a { opacity: 1 }\n.a:hover { opacity: 0.5 }";
  assert.deepEqual(stylesFor(css, {
    elementType: "border",
    classes: ["a"]
  }), [["Opacity", "1"]]);
  assert.deepEqual(stylesFor(css, {
    elementType: "border",
    classes: ["a"],
    states: ["hover"]
  }), [["Opacity", "0.5"]]);
});

test("cascade: the element's own style beats every rule", () => {
  assert.deepEqual(stylesFor(".a { opacity: 1 !important; width: 10px }", {
    elementType: "border",
    classes: ["a"]
  }, "width: 20px; opacity: 0"), [["Opacity", "1"], ["Width", "20"]]);
});

test("a Style as XAML: based on, setters, element-form setters", () => {
  const xaml = styleXaml("K", "text-block", {
    basedOn: "CaptionTextBlockStyle",
    setters: [
      {
        kind: "setter",
        property: "Foreground",
        value: "{ThemeResource TextFillColorSecondaryBrush}"
      },
      {
        kind: "content",
        property: "OpacityTransition",
        xaml: "<ScalarTransition Duration=\"0:0:0.200\"/>"
      },
      {
        kind: "setter",
        property: "FontFamily",
        value: "A & \"B\""
      }
    ],
    writes: []
  });
  assert.equal(xaml, [
    "<Style x:Key=\"K\" TargetType=\"TextBlock\" BasedOn=\"{StaticResource CaptionTextBlockStyle}\">",
    "  <Setter Property=\"Foreground\" Value=\"{ThemeResource TextFillColorSecondaryBrush}\"/>",
    "  <Setter Property=\"OpacityTransition\"><Setter.Value><ScalarTransition Duration=\"0:0:0.200\"/></Setter.Value></Setter>",
    "  <Setter Property=\"FontFamily\" Value=\"A &amp; &quot;B&quot;\"/>",
    "</Style>"
  ].join("\n"));
});
