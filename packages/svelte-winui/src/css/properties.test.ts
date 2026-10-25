import { computeDeclarations } from "./cascade.ts";
import { loadWinuiCatalogue } from "./catalogue.ts";
import { lowerDeclarations } from "./properties.ts";
import { formatDiagnostic, translateStyleSheet } from "./translate.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// Every row of the property map, through the same path a component's CSS takes:
// translated, then lowered for an element type.

const resources = loadWinuiCatalogue();

interface Lowered {
  setters: Array<[string, string]>;
  writes: Array<[string, string]>;
  basedOn: string | null;
  errors: string[];
  warnings: string[];
}

function lower(elementType: string | null, declarations: string): Lowered {
  const selector = elementType === null ? ".t" : `${elementType}.t`;
  const translation = translateStyleSheet({
    file: "t.svelte",
    css: `${selector} { ${declarations} }`,
    resources
  });
  const rule = translation.sheet.rules[0];
  const messages: Lowered = {
    setters: [],
    writes: [],
    basedOn: null,
    errors: translation.diagnostics.filter(diagnostic => diagnostic.severity === "error").map(formatDiagnostic),
    warnings: translation.diagnostics.filter(diagnostic => diagnostic.severity === "warning").map(formatDiagnostic)
  };
  if (rule === undefined) {
    return messages;
  }

  const quiet = {
    elementType,
    resolveVariable: () => ({
      isFound: false as const,
      reason: "unused"
    }),
    hasStyle: () => true,
    report: () => {}
  };
  const lowered = lowerDeclarations(computeDeclarations([rule], []), {
    ...quiet,
    resolveVariable: (name, kind) => {
      const base = name.replace(/^--/u, "");
      return base === "" ? quiet.resolveVariable() : {
        isFound: true as const,
        key: `${base}:${kind}`
      };
    }
  });
  messages.setters = lowered.setters.map(entry => [entry.property, entry.kind === "setter" ? entry.value : entry.xaml]);
  messages.writes = lowered.writes.map(entry => [entry.target, entry.value]);
  messages.basedOn = lowered.basedOn;
  return messages;
}

function setters(elementType: string | null, declarations: string): Array<[string, string]> {
  const result = lower(elementType, declarations);
  assert.deepEqual(result.errors, [], declarations);
  return result.setters;
}

const rows: Array<[string, string | null, string, Array<[string, string]>]> = [
  ["margin, CSS side order to a Thickness", null, "margin: 0 12px", [["Margin", "12,0,12,0"]]],
  ["margin, one value", null, "margin: 8px", [["Margin", "8,8,8,8"]]],
  ["padding, two values", null, "padding: 12px 16px", [["Padding", "16,12,16,12"]]],
  ["padding, three values", null, "padding: 1px 2px 3px", [["Padding", "2,1,2,3"]]],
  ["padding, four values", null, "padding: 1px 2px 3px 4px", [["Padding", "4,1,2,3"]]],
  ["padding, one side", null, "padding-left: 4px", [["Padding", "4,0,0,0"]]],
  ["padding, rem is 16px", null, "padding: 0.5rem", [["Padding", "8,8,8,8"]]],
  ["gap on a vertical stack", "stack-panel", "gap: 6px", [["Spacing", "6"]]],
  ["gap on a horizontal stack takes the column gap", "stack-panel", "display: flex; gap: 2px 8px", [
    ["Orientation", "Horizontal"],
    ["Spacing", "8"]
  ]],
  ["gap on a column flex takes the row gap", "stack-panel", "display: flex; flex-direction: column; gap: 2px 8px", [
    ["Orientation", "Vertical"],
    ["Spacing", "2"]
  ]],
  ["gap on a grid", "grid", "gap: 4px 8px", [
    ["RowSpacing", "4"],
    ["ColumnSpacing", "8"]
  ]],
  ["flex-direction alone", "stack-panel", "flex-direction: row", [["Orientation", "Horizontal"]]],
  ["align-self to VerticalAlignment", null, "align-self: center", [["VerticalAlignment", "Center"]]],
  ["align-self end", null, "align-self: end", [["VerticalAlignment", "Bottom"]]],
  ["justify-self start", null, "justify-self: start", [["HorizontalAlignment", "Left"]]],
  ["justify-self right", null, "justify-self: right", [["HorizontalAlignment", "Right"]]],
  ["place-self", null, "place-self: center start", [
    ["VerticalAlignment", "Center"],
    ["HorizontalAlignment", "Left"]
  ]],
  ["width, height", null, "width: 120px; height: 2rem", [
    ["Width", "120"],
    ["Height", "32"]
  ]],
  ["auto is NaN", null, "width: auto", [["Width", "NaN"]]],
  ["min and max", null, "min-width: 0; min-height: 68px; max-width: none; max-height: 300px", [
    ["MinWidth", "0"],
    ["MinHeight", "68"],
    ["MaxWidth", "Infinity"],
    ["MaxHeight", "300"]
  ]],
  ["opacity", null, "opacity: 0.5", [["Opacity", "0.5"]]],
  ["opacity as a percentage", null, "opacity: 40%", [["Opacity", "0.4"]]],
  ["color to Foreground", null, "color: #0af", [["Foreground", "#00AAFF"]]],
  ["color from a WinUI brush", null, "color: var(--text-fill-color-secondary)", [["Foreground", "{ThemeResource text-fill-color-secondary:Brush}"]]],
  ["background-color", null, "background-color: rgb(0 0 0 / 50%)", [["Background", "#80000000"]]],
  ["background to a brush", null, "background: transparent", [["Background", "#00FFFFFF"]]],
  ["background none", null, "background: none", [["Background", "#00FFFFFF"]]],
  ["border shorthand", null, "border: 1px solid red", [
    ["BorderThickness", "1,1,1,1"],
    ["BorderBrush", "#FF0000"]
  ]],
  ["border none", null, "border: none", [["BorderThickness", "0,0,0,0"]]],
  ["one side's border", null, "border-bottom: 2px solid blue", [
    ["BorderThickness", "0,0,0,2"],
    ["BorderBrush", "#0000FF"]
  ]],
  ["border-width and border-color", null, "border-width: 1px 2px; border-style: solid; border-color: green", [
    ["BorderThickness", "2,1,2,1"],
    ["BorderBrush", "#008000"]
  ]],
  ["border-radius, one value", null, "border-radius: 4px", [["CornerRadius", "4,4,4,4"]]],
  ["border-radius, corners clockwise from top left", null, "border-radius: 1px 2px 3px 4px", [["CornerRadius", "1,2,3,4"]]],
  ["border-radius from a WinUI resource", null, "border-radius: var(--control-corner-radius)", [["CornerRadius", "{ThemeResource control-corner-radius:CornerRadius}"]]],
  ["one corner", null, "border-top-left-radius: 8px", [["CornerRadius", "8,0,0,0"]]],
  ["font-size", null, "font-size: 14px", [["FontSize", "14"]]],
  ["font-weight number", null, "font-weight: 600", [["FontWeight", "SemiBold"]]],
  ["font-weight keyword", null, "font-weight: bold", [["FontWeight", "Bold"]]],
  ["font-weight rounds to the nearest", null, "font-weight: 340", [["FontWeight", "SemiLight"]]],
  ["font-style", null, "font-style: italic", [["FontStyle", "Italic"]]],
  ["font-family with generics", null, "font-family: \"Segoe UI Variable\", monospace", [["FontFamily", "Segoe UI Variable, Consolas"]]],
  ["line-height in px fixes the line", null, "line-height: 20px", [
    ["LineHeight", "20"],
    ["LineStackingStrategy", "BlockLineHeight"]
  ]],
  ["font shorthand", null, "font: italic 600 14px/20px \"Segoe UI\"", [
    ["FontStyle", "Italic"],
    ["FontWeight", "SemiBold"],
    ["FontSize", "14"],
    ["LineHeight", "20"],
    ["LineStackingStrategy", "BlockLineHeight"],
    ["FontFamily", "Segoe UI"]
  ]],
  ["letter-spacing in em", null, "letter-spacing: 0.05em", [["CharacterSpacing", "50"]]],
  ["text-align", null, "text-align: center", [["TextAlignment", "Center"]]],
  ["text-wrap", null, "text-wrap: nowrap", [["TextWrapping", "NoWrap"]]],
  ["white-space", null, "white-space: normal", [["TextWrapping", "Wrap"]]],
  ["text-overflow", null, "text-overflow: ellipsis", [["TextTrimming", "CharacterEllipsis"]]],
  ["text-decoration", null, "text-decoration: underline", [["TextDecorations", "Underline"]]],
  ["visibility hidden collapses", null, "visibility: hidden", [["Visibility", "Collapsed"]]],
  ["display none collapses", null, "display: none", [["Visibility", "Collapsed"]]],
  ["display none wins over visible", null, "visibility: visible; display: none", [["Visibility", "Collapsed"]]],
  ["display block adds nothing", null, "display: block", []],
  ["display flex is a horizontal stack", "stack-panel", "display: flex", [["Orientation", "Horizontal"]]],
  ["display grid on a grid", "grid", "display: grid", []],
  ["pointer-events", null, "pointer-events: none", [["IsHitTestVisible", "False"]]],
  ["z-index", null, "z-index: 3", [["Canvas.ZIndex", "3"]]],
  ["translate", null, "translate: 0 4px", [["Translation", "0,4,0"]]],
  ["scale", null, "scale: 1.1", [["Scale", "1.1,1.1,1"]]],
  ["rotate", null, "rotate: 45deg", [["Rotation", "45"]]],
  ["grid-column with a span", "grid", "grid-column: 2 / span 2", [
    ["Grid.Column", "1"],
    ["Grid.ColumnSpan", "2"]
  ]],
  ["grid-row from line to line", "grid", "grid-row: 1 / 3", [
    ["Grid.Row", "0"],
    ["Grid.RowSpan", "2"]
  ]],
  ["span alone", "grid", "grid-column: span 3", [["Grid.ColumnSpan", "3"]]],
  ["overflow on a scroll viewer", "scroll-viewer", "overflow-y: auto; overflow-x: hidden", [
    ["HorizontalScrollBarVisibility", "Disabled"],
    ["VerticalScrollBarVisibility", "Auto"]
  ]],
  ["transition on opacity", null, "transition: opacity 200ms", [["OpacityTransition", "<ScalarTransition Duration=\"0:0:0.200\"/>"]]],
  ["transitions in a list", null, "transition: opacity 0.2s, translate 100ms, background-color 1s", [
    ["OpacityTransition", "<ScalarTransition Duration=\"0:0:0.200\"/>"],
    ["TranslationTransition", "<Vector3Transition Duration=\"0:0:0.100\"/>"],
    ["BackgroundTransition", "<BrushTransition Duration=\"0:0:1.000\"/>"]
  ]]
];

for (const [name, elementType, declarations, expected] of rows) {
  test(`${name}: ${declarations}`, () => {
    // The order of setters in a Style does not matter.
    assert.deepEqual(setters(elementType, declarations).sort(), expected.toSorted());
  });
}

test("a later declaration beats an earlier one, and !important beats a later one", () => {
  assert.deepEqual(setters(null, "padding: 1px; padding: 2px"), [["Padding", "2,2,2,2"]]);
  assert.deepEqual(setters(null, "opacity: 1 !important; opacity: 0.5"), [["Opacity", "1"]]);
});

test("grid templates are written to the grid, not set by a Style", () => {
  const result = lower("grid", "grid-template-columns: auto 1fr 2fr 120px; grid-template-rows: repeat(2, auto)");
  assert.deepEqual(result.errors, []);
  assert.deepEqual(result.writes, [
    ["columns", "Auto,*,2*,120"],
    ["rows", "Auto,Auto"]
  ]);
  assert.deepEqual(lower("grid", "grid-template-columns: none").writes, [["columns", ""]]);
});

test("-winui-based-on names a WinUI style", () => {
  assert.equal(lower("text-block", "-winui-based-on: BodyTextBlockStyle").basedOn, "BodyTextBlockStyle");
});

test("cursor is ignored with a warning, not an error", () => {
  const result = lower(null, "cursor: pointer; opacity: 1");
  assert.deepEqual(result.errors, []);
  assert.equal(result.warnings.length, 1);
  assert.match(result.warnings[0] ?? "", /^t\.svelte:1:\d+: warning: cursor ignored/u);
  assert.deepEqual(result.setters, [["Opacity", "1"]]);
});

test("a transition WinUI cannot animate warns and is dropped", () => {
  const result = lower(null, "transition: color 1s, opacity 1s ease-out");
  assert.deepEqual(result.errors, []);
  assert.equal(result.warnings.length, 2);
  assert.deepEqual(result.setters.map(([property]) => property), ["OpacityTransition"]);
});

const failures: Array<[string, string | null, string, RegExp]> = [
  ["a property WinUI has no counterpart for", null, "box-shadow: 0 0 4px red", /^t\.svelte:1:6: box-shadow has no WinUI meaning \(a WinUI shadow is a ThemeShadow/u],
  ["an unknown property", null, "frobnicate: 1", /frobnicate has no WinUI meaning \(WinUI has no property that corresponds to it\)/u],
  ["flex sizing of a child", null, "flex: 1", /flex has no WinUI meaning \(a child cannot size itself in its parent: give the parent grid-template-columns with fr tracks\)/u],
  ["align-items on a container", null, "align-items: center", /align-items has no WinUI meaning \(WinUI aligns a child by its own VerticalAlignment: set align-self on the child\)/u],
  ["a length without a unit", null, "width: 12", /width: expected a length in px/u],
  ["a percentage length", null, "width: 50%", /width: expected a length in px/u],
  ["a bad color", null, "color: sparkly", /color: expected a color/u],
  ["a gradient background", null, "background: linear-gradient(red, blue)", /images and gradients have no Style value in WinUI/u],
  ["a unitless line height", null, "line-height: 1.5", /a multiple of the font size has no WinUI meaning/u],
  ["opacity out of range", null, "opacity: 2", /opacity: expected a number or percentage from 0 to 1/u],
  ["a dashed border", null, "border: 1px dashed red", /border: dashed has no WinUI meaning \(a WinUI border is solid/u],
  ["border without a style", null, "border: 1px red", /border: expected a style/u],
  ["elliptical corners", null, "border-radius: 10px / 20px", /elliptical corners/u],
  ["gap on a border", "border", "gap: 4px", /gap has no WinUI meaning on <border>/u],
  ["display flex on a grid", "grid", "display: flex", /display: flex has no WinUI meaning on <grid>/u],
  ["display grid on a stack", "stack-panel", "display: grid", /display: grid has no WinUI meaning on <stack-panel>/u],
  ["minmax in a template", "grid", "grid-template-columns: minmax(10px, 1fr)", /minmax\(\) has no WinUI meaning/u],
  ["overflow on a border", "border", "overflow: hidden", /overflow-x: hidden has no WinUI meaning on <border>/u],
  ["letter spacing in px", null, "letter-spacing: 2px", /expected normal or a size in em/u],
  ["an unknown base style", null, "-winui-based-on: NopeStyle", /NopeStyle is not a style WinUI defines/u]
];

for (const [name, elementType, declarations, pattern] of failures) {
  test(`fails the build: ${name}`, () => {
    const result = lower(elementType, declarations);
    assert.ok(result.errors.length > 0, `no error for ${declarations}`);
    assert.match(result.errors.join("\n"), pattern);
  });
}
