import type { LowerOptions } from "./reader.ts";
import { NotANumber, Reader, Unbounded } from "./reader.ts";
import type { Declaration } from "./types.ts";
import { parseLength, parseMilliseconds, parseNumber, parsePercent, splitCommas, splitWords } from "./values.ts";

// THE property map: which CSS property becomes which WinUI property, and how
// its value is spelled. One table of groups (a group is the CSS properties
// that together set one WinUI property, such as padding's four sides), read
// by `lowerDeclarations`. A property absent here has no WinUI meaning and
// fails the build; `unsupportedReasons` says why for the likely ones.

export const ElementType = {
  StackPanel: "stack-panel",
  Grid: "grid",
  ScrollViewer: "scroll-viewer"
} as const;

// A Style setter, a collection a Style cannot set, or the style this one
// builds on.
export type Output =
  | { kind: "setter"; property: string; value: string }
  | { kind: "content"; property: string; xaml: string }
  | { kind: "write"; target: "columns" | "rows"; value: string }
  | { kind: "basedOn"; key: string };

export interface LoweredStyle {
  basedOn: string | null;
  setters: Array<Extract<Output, { kind: "setter" | "content" }>>;
  writes: Array<Extract<Output, { kind: "write" }>>;
}

interface GroupInput {
  options: LowerOptions;
  computed: ReadonlyMap<string, Declaration>;
  readers: ReadonlyMap<string, Reader>;
}

interface PropertyGroup {
  members: readonly string[];
  lower(input: GroupInput): Output[];
}

const sides = ["top", "right", "bottom", "left"] as const;
const corners = ["top-left", "top-right", "bottom-right", "bottom-left"] as const;
const dipsPerEm = 1000;
const dipsToTicksSeconds = 1000;

function setter(property: string, value: string): Output {
  return {
    kind: "setter",
    property,
    value
  };
}

function enumMap(entries: ReadonlyArray<[string, string]>): ReadonlyMap<string, string> {
  return new Map(entries);
}

function reading(input: GroupInput, member: string): Reader {
  const reader = input.readers.get(member);
  if (reader === undefined) {
    throw new RangeError(`${member} is not present`);
  }

  return reader;
}

// A group of one CSS property to one WinUI property.
function simple(member: string, property: string, parse: (reader: Reader) => string | null): PropertyGroup {
  return {
    members: [member],
    lower(input) {
      const value = parse(reading(input, member));
      return value === null ? [] : [setter(property, value)];
    }
  };
}

function brushProperty(member: string, property: string): PropertyGroup {
  return simple(member, property, reader => {
    const word = reader.single();
    return word === null ? null : reader.brush(word);
  });
}

function lengthProperty(member: string, property: string, extra: ReadonlyMap<string, string> = new Map()): PropertyGroup {
  return simple(member, property, reader => {
    const word = reader.single();
    if (word === null) {
      return null;
    }

    return extra.get(word.toLowerCase()) ?? reader.length(word);
  });
}

function keywordProperty(member: string, property: string, choices: ReadonlyMap<string, string>): PropertyGroup {
  return simple(member, property, reader => {
    const word = reader.single();
    return word === null ? null : reader.keyword(word, choices);
  });
}

// padding and margin: four sides, 0 for a side left out; or one var() holding
// the whole Thickness.
function thicknessGroup(name: string, property: string): PropertyGroup {
  const members = [name, ...sides.map(side => `${name}-${side}`)];
  return {
    members,
    lower(input) {
      const whole = input.readers.get(name);
      if (whole !== undefined) {
        const word = whole.single();
        const resource = word === null ? null : whole.variable(word, "Thickness");
        return typeof resource === "string" ? [setter(property, resource)] : [];
      }

      const [top, right, bottom, left] = sides.map(side => {
        const reader = input.readers.get(`${name}-${side}`);
        const word = reader?.single();
        return reader === undefined || word === null || word === undefined ? 0 : reader.pixels(word) ?? 0;
      });
      return [setter(property, `${left},${top},${right},${bottom}`)];
    }
  };
}

function borderThickness(): PropertyGroup {
  const members = ["border-width", "border-style", ...sides.map(side => `border-${side}-width`)];
  return {
    members,
    lower(input) {
      const whole = input.readers.get("border-width");
      if (whole !== undefined) {
        const word = whole.single();
        const resource = word === null ? null : whole.variable(word, "Thickness");
        return typeof resource === "string" ? [setter("BorderThickness", resource)] : [];
      }

      const isNone = ["none", "hidden"].includes(input.readers.get("border-style")?.declaration.value.toLowerCase() ?? "");
      const [top, right, bottom, left] = sides.map(side => {
        const reader = input.readers.get(`border-${side}-width`);
        const word = reader?.single();
        const isMeasured = reader !== undefined && word !== null && word !== undefined && !isNone;
        return isMeasured ? reader.pixels(word) ?? 0 : 0;
      });
      const hasBorder = input.readers.has("border-style") || sides.some(side => input.readers.has(`border-${side}-width`));
      return hasBorder ? [setter("BorderThickness", `${left},${top},${right},${bottom}`)] : [];
    }
  };
}

function cornerRadius(): PropertyGroup {
  const members = ["border-radius", ...corners.map(corner => `border-${corner}-radius`)];
  return {
    members,
    lower(input) {
      const whole = input.readers.get("border-radius");
      if (whole !== undefined) {
        const word = whole.single();
        const resource = word === null ? null : whole.variable(word, "CornerRadius");
        return typeof resource === "string" ? [setter("CornerRadius", resource)] : [];
      }

      const [topLeft, topRight, bottomRight, bottomLeft] = corners.map(corner => {
        const reader = input.readers.get(`border-${corner}-radius`);
        const word = reader?.single();
        return reader === undefined || word === null || word === undefined ? 0 : reader.pixels(word) ?? 0;
      });
      return [setter("CornerRadius", `${topLeft},${topRight},${bottomRight},${bottomLeft}`)];
    }
  };
}

const fontWeights = [
  [100, "Thin"],
  [200, "ExtraLight"],
  [300, "Light"],
  [350, "SemiLight"],
  [400, "Normal"],
  [500, "Medium"],
  [600, "SemiBold"],
  [700, "Bold"],
  [800, "ExtraBold"],
  [900, "Black"],
  [950, "ExtraBlack"]
] as const;
const fontWeightKeywords = enumMap([
  ["normal", "Normal"],
  ["bold", "Bold"]
]);

function fontWeight(reader: Reader): string | null {
  const word = reader.single();
  if (word === null) {
    return null;
  }

  const fromVariable = reader.variable(word, "FontWeight");
  if (fromVariable !== undefined) {
    return fromVariable;
  }

  const keyword = fontWeightKeywords.get(word.toLowerCase());
  if (keyword !== undefined) {
    return keyword;
  }

  const weight = parseNumber(word);
  if (weight === null) {
    return reader.expected("normal, bold, or a weight from 1 to 1000", word);
  }

  const nearest = fontWeights.reduce((best, candidate) =>
    Math.abs(candidate[0] - weight) < Math.abs(best[0] - weight) ? candidate : best);
  return nearest[1];
}

const genericFamilies = enumMap([
  ["system-ui", "Segoe UI Variable"],
  ["sans-serif", "Segoe UI"],
  ["serif", "Cambria"],
  ["monospace", "Consolas"],
  ["ui-monospace", "Consolas"]
]);

function fontFamily(reader: Reader): string | null {
  const text = reader.declaration.value;
  const fromVariable = reader.variable(text, "FontFamily");
  if (fromVariable !== undefined) {
    return fromVariable;
  }

  const names = splitCommas(text).map(name => {
    const unquoted = name.replace(/^(["'])(.*)\1$/u, "$2");
    return genericFamilies.get(unquoted.toLowerCase()) ?? unquoted;
  });
  return names.join(", ");
}

function lineHeight(input: GroupInput): Output[] {
  const reader = reading(input, "line-height");
  const word = reader.single();
  if (word === null) {
    return [];
  }

  if (word.toLowerCase() === "normal") {
    return [setter("LineHeight", "0")];
  }

  const isUnitless = parseNumber(word) !== null && parseNumber(word) !== 0;
  if (isUnitless) {
    reader.error(`line-height ${word}: a multiple of the font size has no WinUI meaning; write the height in px`);
    return [];
  }

  const value = reader.length(word);
  // LineHeight only fixes the line's height with this strategy.
  return value === null ? [] : [setter("LineHeight", value), setter("LineStackingStrategy", "BlockLineHeight")];
}

function opacity(reader: Reader): string | null {
  const word = reader.single();
  if (word === null) {
    return null;
  }

  const amount = parseNumber(word) ?? parsePercent(word);
  const isInRange = amount !== null && amount >= 0 && amount <= 1;
  return isInRange ? String(amount) : reader.expected("a number or percentage from 0 to 1", word);
}

function letterSpacing(reader: Reader): string | null {
  const word = reader.single();
  if (word === null) {
    return null;
  }

  const isNormal = word.toLowerCase() === "normal" || word === "0";
  if (isNormal) {
    return "0";
  }

  const match = /^(-?\d*\.?\d+)em$/iu.exec(word);
  // WinUI's CharacterSpacing counts thousandths of an em.
  return match === null ? reader.expected("normal or a size in em (WinUI spaces by em)", word) : String(Number(match[1]) * dipsPerEm);
}

function gridTemplate(target: "columns" | "rows", member: string): PropertyGroup {
  return {
    members: [member],
    lower(input) {
      const reader = reading(input, member);
      const tracks = splitWords(reader.declaration.value).flatMap(repeated);
      const lengths = tracks.map(track => gridLength(track, reader));
      const isNone = reader.declaration.value.toLowerCase() === "none";
      if (isNone || lengths.includes(null)) {
        return isNone ? [{
          kind: "write",
          target,
          value: ""
        }] : [];
      }

      return [{
        kind: "write",
        target,
        value: lengths.join(",")
      }];
    }
  };
}

function repeated(track: string): string[] {
  const match = /^repeat\(\s*(\d+)\s*,(.*)\)$/isu.exec(track);
  if (match === null) {
    return [track];
  }

  const count = Number(match[1]);
  return Array.from({ length: count }, () => splitWords(match[2] ?? "")).flat();
}

// A track as the renderer's `columns="Auto,*,2*,120"` spells it.
function gridLength(track: string, reader: Reader): string | null {
  const lower = track.toLowerCase();
  if (lower === "auto") {
    return "Auto";
  }

  const fraction = /^(\d*\.?\d+)fr$/u.exec(lower);
  if (fraction !== null) {
    return Number(fraction[1]) === 1 ? "*" : `${fraction[1]}*`;
  }

  const pixels = parseLength(track);
  if (pixels !== null) {
    return String(pixels);
  }

  const isMinMax = lower.startsWith("minmax(");
  const hint = isMinMax ? "minmax() has no WinUI meaning (a track is Auto, a size or a star)" : "an auto, a length in px, or a size in fr";
  return reader.expected(hint, track);
}

const gridLines = new Map<string, [string, string]>([
  ["grid-column", ["Grid.Column", "Grid.ColumnSpan"]],
  ["grid-row", ["Grid.Row", "Grid.RowSpan"]]
]);

function gridPlacement(axis: "grid-column" | "grid-row"): PropertyGroup {
  const [position = "", span = ""] = gridLines.get(axis) ?? [];
  const startMember = `${axis}-start`;
  const endMember = `${axis}-end`;
  return {
    members: [startMember, endMember],
    lower(input) {
      const outputs: Output[] = [];
      const startReader = input.readers.get(startMember);
      const endReader = input.readers.get(endMember);
      const start = parseLine(startReader);
      const end = parseLine(endReader);
      if (start?.span !== undefined) {
        outputs.push(setter(span, String(start.span)));
      } else if (start?.line !== undefined) {
        outputs.push(setter(position, String(start.line - 1)));
      }

      if (end?.span !== undefined) {
        outputs.push(setter(span, String(end.span)));
      } else if (end?.line !== undefined && start?.line !== undefined) {
        outputs.push(setter(span, String(end.line - start.line)));
      }

      return outputs;
    }
  };
}

function parseLine(reader: Reader | undefined): { line?: number; span?: number } | undefined {
  if (reader === undefined) {
    return undefined;
  }

  const value = reader.declaration.value.trim().toLowerCase();
  if (value === "auto") {
    return {};
  }

  const spanMatch = /^span\s+(\d+)$/u.exec(value);
  if (spanMatch !== null) {
    return { span: Number(spanMatch[1]) };
  }

  const line = parseNumber(value);
  const isLine = line !== null && Number.isInteger(line) && line >= 1;
  if (isLine) {
    return { line };
  }

  reader.expected("a positive line number, `span n` or auto");
  return undefined;
}

const alignments = enumMap([
  ["start", "Start"],
  ["flex-start", "Start"],
  ["end", "End"],
  ["flex-end", "End"],
  ["center", "Center"],
  ["stretch", "Stretch"],
  ["normal", "Stretch"],
  ["auto", "Stretch"]
]);

// CSS's start and end are WinUI's Top and Bottom, or Left and Right.
function alignmentProperty(member: string, property: string, startName: string, endName: string): PropertyGroup {
  const names = new Map(alignments);
  const horizontalOnly = property === "HorizontalAlignment";
  if (horizontalOnly) {
    names.set("left", "Start");
    names.set("right", "End");
  }

  return simple(member, property, reader => {
    const word = reader.single();
    const found = word === null ? null : reader.keyword(word, names);
    if (found === null) {
      return null;
    }

    if (found === "Start") {
      return startName;
    }

    return found === "End" ? endName : found;
  });
}

function orientationOf(computed: ReadonlyMap<string, Declaration>): "Horizontal" | "Vertical" | null {
  const direction = computed.get("flex-direction")?.value.toLowerCase();
  if (direction === "column") {
    return "Vertical";
  }

  if (direction === "row") {
    return "Horizontal";
  }

  const display = computed.get("display")?.value.toLowerCase() ?? "";
  return ["flex", "inline-flex"].includes(display) ? "Horizontal" : null;
}

function describeType(elementType: string | null): string {
  return elementType === null ? "this element" : `<${elementType}>`;
}

const flexDirections = enumMap([
  ["row", "Horizontal"],
  ["column", "Vertical"]
]);

const displayKeywords = new Set(["none", "block", "inline", "inline-block", "flex", "inline-flex", "grid", "inline-grid", "contents"]);

function display(input: GroupInput): Output[] {
  const reader = reading(input, "display");
  const word = reader.single()?.toLowerCase();
  if (word === undefined) {
    return [];
  }

  if (!displayKeywords.has(word)) {
    reader.expected(`one of ${[...displayKeywords].join(", ")}`, word);
    return [];
  }

  if (word === "none") {
    return [setter("Visibility", "Collapsed")];
  }

  const { elementType } = input.options;
  const isFlex = word.endsWith("flex");
  const isGrid = word.endsWith("grid");
  const isKnownType = elementType !== null;
  const isFlexOnStack = elementType === ElementType.StackPanel;
  const isGridOnGrid = elementType === ElementType.Grid;
  if (isFlex && isKnownType && !isFlexOnStack) {
    reader.error(`display: ${word} has no WinUI meaning on ${describeType(elementType)} (a flex row or column is a <stack-panel>; use grid for two dimensions)`);
    return [];
  }

  if (isGrid && isKnownType && !isGridOnGrid) {
    reader.error(`display: ${word} has no WinUI meaning on ${describeType(elementType)} (only a <grid> lays out in rows and columns)`);
    return [];
  }

  // flex-direction names the orientation when it is there.
  const hasDirection = input.computed.has("flex-direction");
  return isFlex && isFlexOnStack && !hasDirection ? [setter("Orientation", "Horizontal")] : [];
}

function gap(input: GroupInput): Output[] {
  const { elementType } = input.options;
  const rowReader = input.readers.get("row-gap");
  const columnReader = input.readers.get("column-gap");
  const row = rowReader === undefined ? null : rowReader.length(rowReader.single() ?? "");
  const column = columnReader === undefined ? null : columnReader.length(columnReader.single() ?? "");
  if (elementType === ElementType.Grid) {
    return [
      ...row === null ? [] : [setter("RowSpacing", row)],
      ...column === null ? [] : [setter("ColumnSpacing", column)]
    ];
  }

  if (elementType === ElementType.StackPanel) {
    // A stack spaces its children along its orientation.
    const isVertical = (orientationOf(input.computed) ?? "Vertical") === "Vertical";
    const along = isVertical ? row : column;
    return along === null ? [] : [setter("Spacing", along)];
  }

  const reader = rowReader ?? columnReader;
  if (elementType !== null && reader !== undefined) {
    reader.error(`gap has no WinUI meaning on ${describeType(elementType)} (only <stack-panel> has Spacing and <grid> has RowSpacing and ColumnSpacing)`);
  }

  return [];
}

const scrollBarVisibilities = enumMap([
  ["auto", "Auto"],
  ["scroll", "Visible"],
  ["hidden", "Disabled"],
  ["clip", "Disabled"]
]);

function overflow(input: GroupInput): Output[] {
  const outputs: Output[] = [];
  const axes: Array<[string, string]> = [
    ["overflow-x", "HorizontalScrollBarVisibility"],
    ["overflow-y", "VerticalScrollBarVisibility"]
  ];
  for (const [member, property] of axes) {
    const reader = input.readers.get(member);
    const word = reader?.single()?.toLowerCase();
    if (reader === undefined || word === undefined || word === "visible") {
      continue;
    }

    const visibility = reader.keyword(word, scrollBarVisibilities);
    const { elementType } = input.options;
    if (visibility !== null && elementType !== null && elementType !== ElementType.ScrollViewer) {
      reader.error(`${member}: ${word} has no WinUI meaning on ${describeType(elementType)} (scrolling belongs to a <scroll-viewer>; WinUI has no Style value for clipping)`);
    } else if (visibility !== null) {
      outputs.push(setter(property, visibility));
    }
  }

  return outputs;
}

function translation(reader: Reader): Output[] {
  const words = reader.words;
  const [first, second = "0", third = "0"] = words;
  const lengths = [first, second, third].map(word => (word === undefined ? null : parseLength(word)));
  const isValid = words.length >= 1 && words.length <= 3 && !lengths.includes(null);
  if (!isValid) {
    reader.expected("one to three lengths");
    return [];
  }

  return [setter("Translation", lengths.join(","))];
}

function scaling(reader: Reader): Output[] {
  const [first, second = first, third = "1"] = reader.words;
  const factors = [first, second, third].map(word => (word === undefined ? null : parseNumber(word)));
  if (factors.includes(null)) {
    reader.expected("one to three numbers");
    return [];
  }

  return [setter("Scale", factors.join(","))];
}

function rotation(reader: Reader): Output[] {
  const word = reader.single() ?? "";
  const match = /^(-?\d*\.?\d+)deg$/iu.exec(word);
  if (match === null) {
    reader.expected("an angle in deg", word);
    return [];
  }

  return [setter("Rotation", String(Number(match[1])))];
}

// WinUI animates these properties by an implicit transition set on them.
const transitionTargets = new Map<string, Array<[string, string]>>([
  ["opacity", [["OpacityTransition", "ScalarTransition"]]],
  ["translate", [["TranslationTransition", "Vector3Transition"]]],
  ["scale", [["ScaleTransition", "Vector3Transition"]]],
  ["rotate", [["RotationTransition", "ScalarTransition"]]],
  ["background", [["BackgroundTransition", "BrushTransition"]]],
  ["background-color", [["BackgroundTransition", "BrushTransition"]]],
  ["all", [
    ["OpacityTransition", "ScalarTransition"],
    ["TranslationTransition", "Vector3Transition"],
    ["ScaleTransition", "Vector3Transition"],
    ["RotationTransition", "ScalarTransition"]
  ]]
]);

function duration(milliseconds: number): string {
  const seconds = (milliseconds / dipsToTicksSeconds).toFixed(3);
  return `0:0:${seconds}`;
}

function transition(input: GroupInput): Output[] {
  const reader = reading(input, "transition");
  const outputs: Output[] = [];
  for (const entry of splitCommas(reader.declaration.value)) {
    const [property = "", time, ...rest] = splitWords(entry);
    const milliseconds = time === undefined ? null : parseMilliseconds(time);
    const targets = transitionTargets.get(property.toLowerCase());
    if (targets === undefined) {
      reader.warn(`transition: ${property} cannot be animated (WinUI animates opacity, translate, scale, rotate and background); ignored`);
      continue;
    }

    if (milliseconds === null) {
      reader.expected("`property duration`", entry);
      continue;
    }

    if (rest.length > 0) {
      reader.warn(`transition: "${rest.join(" ")}" ignored (WinUI's implicit transitions take a duration only)`);
    }

    for (const [name, type] of targets) {
      outputs.push({
        kind: "content",
        property: name,
        xaml: `<${type} Duration="${duration(milliseconds)}"/>`
      });
    }
  }

  return outputs;
}

const textAlignments = enumMap([
  ["left", "Left"],
  ["start", "Left"],
  ["center", "Center"],
  ["right", "Right"],
  ["end", "Right"],
  ["justify", "Justify"]
]);
const wrapKeywords = enumMap([
  ["wrap", "Wrap"],
  ["nowrap", "NoWrap"]
]);
const whiteSpaceKeywords = enumMap([
  ["normal", "Wrap"],
  ["nowrap", "NoWrap"]
]);
const textOverflows = enumMap([
  ["ellipsis", "CharacterEllipsis"],
  ["clip", "Clip"]
]);
const textDecorations = enumMap([
  ["none", "None"],
  ["underline", "Underline"],
  ["line-through", "Strikethrough"]
]);
const fontStyles = enumMap([
  ["normal", "Normal"],
  ["italic", "Italic"],
  ["oblique", "Oblique"]
]);
const visibilities = enumMap([
  ["visible", "Visible"],
  ["hidden", "Collapsed"],
  ["collapse", "Collapsed"]
]);
const hitTesting = enumMap([
  ["none", "False"],
  ["auto", "True"],
  ["all", "True"]
]);

function multiple(member: string, parse: (reader: Reader) => Output[]): PropertyGroup {
  return {
    members: [member],
    lower: input => parse(reading(input, member))
  };
}

const basedOnProperty = "-winui-based-on";

function basedOn(input: GroupInput): Output[] {
  const reader = reading(input, basedOnProperty);
  const key = reader.single();
  if (key === null) {
    return [];
  }

  const isKnown = input.options.hasStyle(key);
  return isKnown ? [{
    kind: "basedOn",
    key
  }] : (reader.error(`${basedOnProperty}: ${key} is not a style WinUI defines (a key such as BodyTextBlockStyle)`), []);
}

function ignored(member: string, why: string): PropertyGroup {
  return {
    members: [member],
    lower(input) {
      reading(input, member).warn(`${member} ignored (${why})`);
      return [];
    }
  };
}

// The groups, in precedence order: where two set the same WinUI property, the
// later one wins (display none over visibility).
const propertyGroups: readonly PropertyGroup[] = [
  { members: [basedOnProperty], lower: basedOn },
  thicknessGroup("margin", "Margin"),
  thicknessGroup("padding", "Padding"),
  borderThickness(),
  brushProperty("border-color", "BorderBrush"),
  cornerRadius(),
  brushProperty("background-color", "Background"),
  brushProperty("color", "Foreground"),
  brushProperty("stroke", "Stroke"),
  lengthProperty("stroke-width", "StrokeThickness"),
  simple("opacity", "Opacity", opacity),
  lengthProperty("width", "Width", new Map([["auto", NotANumber]])),
  lengthProperty("height", "Height", new Map([["auto", NotANumber]])),
  lengthProperty("min-width", "MinWidth", new Map([["auto", "0"]])),
  lengthProperty("min-height", "MinHeight", new Map([["auto", "0"]])),
  lengthProperty("max-width", "MaxWidth", new Map([["none", Unbounded]])),
  lengthProperty("max-height", "MaxHeight", new Map([["none", Unbounded]])),
  lengthProperty("font-size", "FontSize"),
  simple("font-weight", "FontWeight", fontWeight),
  simple("font-family", "FontFamily", fontFamily),
  keywordProperty("font-style", "FontStyle", fontStyles),
  { members: ["line-height"], lower: lineHeight },
  simple("letter-spacing", "CharacterSpacing", letterSpacing),
  keywordProperty("text-align", "TextAlignment", textAlignments),
  keywordProperty("text-wrap", "TextWrapping", wrapKeywords),
  keywordProperty("white-space", "TextWrapping", whiteSpaceKeywords),
  keywordProperty("text-overflow", "TextTrimming", textOverflows),
  keywordProperty("text-decoration", "TextDecorations", textDecorations),
  keywordProperty("text-decoration-line", "TextDecorations", textDecorations),
  alignmentProperty("align-self", "VerticalAlignment", "Top", "Bottom"),
  alignmentProperty("justify-self", "HorizontalAlignment", "Left", "Right"),
  keywordProperty("flex-direction", "Orientation", flexDirections),
  { members: ["row-gap", "column-gap"], lower: gap },
  gridTemplate("columns", "grid-template-columns"),
  gridTemplate("rows", "grid-template-rows"),
  gridPlacement("grid-column"),
  gridPlacement("grid-row"),
  { members: ["overflow-x", "overflow-y"], lower: overflow },
  keywordProperty("visibility", "Visibility", visibilities),
  { members: ["display"], lower: display },
  keywordProperty("pointer-events", "IsHitTestVisible", hitTesting),
  simple("z-index", "Canvas.ZIndex", reader => {
    const value = parseNumber(reader.single() ?? "");
    return value !== null && Number.isInteger(value) ? String(value) : reader.expected("an integer");
  }),
  multiple("translate", translation),
  multiple("scale", scaling),
  multiple("rotate", rotation),
  { members: ["transition"], lower: transition },
  ignored("cursor", "WinUI's controls choose their own cursor")
];

// Why a property no group handles has no WinUI meaning, for the ones an author
// is likely to write.
export const unsupportedReasons: ReadonlyMap<string, string> = new Map([
  ["position", "WinUI lays children out with panels; a <canvas> places by canvas-left and canvas-top"],
  ["top", "WinUI lays children out with panels; a <canvas> places by canvas-top"],
  ["left", "WinUI lays children out with panels; a <canvas> places by canvas-left"],
  ["right", "WinUI lays children out with panels"],
  ["bottom", "WinUI lays children out with panels"],
  ["float", "WinUI has no float; use a grid or a stack-panel"],
  ["flex", "a child cannot size itself in its parent: give the parent grid-template-columns with fr tracks"],
  ["flex-grow", "a child cannot size itself in its parent: give the parent grid-template-columns with fr tracks"],
  ["flex-shrink", "WinUI panels do not shrink their children; give the parent grid-template-columns with fr tracks"],
  ["flex-basis", "a child cannot size itself in its parent: use width or a grid parent"],
  ["flex-wrap", "WinUI has no wrapping stack (WrapPanel is in the Community Toolkit)"],
  ["align-items", "WinUI aligns a child by its own VerticalAlignment: set align-self on the child"],
  ["justify-items", "WinUI aligns a child by its own HorizontalAlignment: set justify-self on the child"],
  ["place-items", "WinUI aligns a child by its own alignment: set align-self and justify-self on the child"],
  ["justify-content", "WinUI aligns a child by its own alignment: set justify-self on the child"],
  ["align-content", "WinUI aligns a child by its own alignment: set align-self on the child"],
  ["box-shadow", "a WinUI shadow is a ThemeShadow set on the element, not a Style value"],
  ["filter", "WinUI has no filters; use an acrylic or Mica backdrop"],
  ["backdrop-filter", "use an acrylic brush or a Mica backdrop"],
  ["transform", "use translate, scale and rotate, which WinUI animates"],
  ["animation", "use transition on opacity, translate, scale or rotate"],
  ["background-image", "a Style cannot hold an image or gradient: use a brush resource through var()"],
  ["content", "WinUI has no generated content"],
  ["outline", "WinUI draws its own focus visuals"],
  ["list-style", "WinUI has no lists to mark"],
  ["text-transform", "WinUI has no text-transform; change the text"],
  ["text-indent", "WinUI has no text indent"],
  ["word-break", "WinUI wraps on words; use text-wrap"],
  ["user-select", "set IsTextSelectionEnabled on the text block"]
]);

export function unsupportedReason(property: string): string {
  return unsupportedReasons.get(property) ?? "WinUI has no property that corresponds to it";
}

const memberToGroup = new Map<string, PropertyGroup>(
  propertyGroups.flatMap(group => group.members.map((member): [string, PropertyGroup] => [member, group]))
);

export function isKnownProperty(property: string): boolean {
  return memberToGroup.has(property);
}

// The final declarations of an element, longhands only (see expandDeclaration),
// as WinUI Style setters.
export function lowerDeclarations(computed: ReadonlyMap<string, Declaration>, options: LowerOptions): LoweredStyle {
  const outputs: Output[] = [];
  for (const group of propertyGroups) {
    const readers = new Map<string, Reader>();
    for (const member of group.members) {
      const declaration = computed.get(member);
      if (declaration !== undefined) {
        readers.set(member, new Reader(declaration, options));
      }
    }

    if (readers.size > 0) {
      outputs.push(...group.lower({
        options,
        computed,
        readers
      }));
    }
  }

  for (const declaration of computed.values()) {
    const isUnknown = !memberToGroup.has(declaration.property) && !declaration.property.startsWith("--");
    if (isUnknown) {
      options.report("error", `${declaration.property} has no WinUI meaning (${unsupportedReason(declaration.property)})`, declaration.at);
    }
  }

  return collect(outputs);
}

// Where two outputs set one property, the later wins.
function collect(outputs: Output[]): LoweredStyle {
  const style: LoweredStyle = {
    basedOn: null,
    setters: [],
    writes: []
  };
  const setters = new Map<string, Extract<Output, { kind: "setter" | "content" }>>();
  for (const output of outputs) {
    if (output.kind === "basedOn") {
      style.basedOn = output.key;
    } else if (output.kind === "write") {
      style.writes.push(output);
    } else {
      setters.delete(output.property);
      setters.set(output.property, output);
    }
  }

  style.setters = [...setters.values()];
  return style;
}
