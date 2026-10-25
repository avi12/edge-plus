import type { LowerOptions } from "./reader.ts";
import { Reader } from "./reader.ts";
import type { Declaration } from "./types.ts";
import { parseLength, parseVariable, splitSlash } from "./values.ts";

// CSS shorthands, expanded into the longhands the property table lowers
// (margin into its four sides, border into width, style and color...). A
// shorthand whose whole value is one `var(--name)` stays as it is: the
// resource holds the complete value (a Thickness, a CornerRadius).

const sides = ["top", "right", "bottom", "left"] as const;
const corners = ["top-left", "top-right", "bottom-right", "bottom-left"] as const;
// CSS's thin, medium and thick, in pixels.
const borderWidthKeywords = new Map([
  ["thin", "1px"],
  ["medium", "3px"],
  ["thick", "5px"]
]);
const borderStyles = new Set(["none", "solid", "hidden"]);
const unsupportedBorderStyles = new Set(["dashed", "dotted", "double", "groove", "ridge", "inset", "outset"]);
const fontStyles = new Set(["normal", "italic", "oblique"]);
const fontWeightWords = new Set(["normal", "bold", "bolder", "lighter"]);

function longhand(declaration: Declaration, property: string, value: string): Declaration {
  return {
    ...declaration,
    property,
    value
  };
}

// CSS's one-to-four value box: top, right, bottom, left (or the four corners).
function box(values: string[]): [string, string, string, string] | null {
  const [first, second = first, third = first, fourth = second] = values;
  const isCounted = values.length >= 1 && values.length <= 4;
  return isCounted && first !== undefined && second !== undefined && third !== undefined && fourth !== undefined
    ? [first, second, third, fourth]
    : null;
}

function isWholeVariable(declaration: Declaration): boolean {
  return parseVariable(declaration.value) !== null;
}

function expandBox(
  declaration: Declaration,
  names: readonly string[],
  reader: Reader,
  template: (name: string) => string
): Declaration[] {
  const values = box(reader.words);
  if (values === null) {
    reader.expected("one to four values");
    return [];
  }

  return names.map((name, index) => longhand(declaration, template(name), values[index] ?? ""));
}

function expandBorder(declaration: Declaration, reader: Reader, sideNames: readonly string[]): Declaration[] {
  let width = "medium";
  let style: string | null = null;
  let color: string | null = null;
  for (const word of reader.words) {
    const lower = word.toLowerCase();
    const isWidth = parseLength(word) !== null || borderWidthKeywords.has(lower);
    if (unsupportedBorderStyles.has(lower)) {
      reader.error(`${reader.declaration.property}: ${lower} has no WinUI meaning (a WinUI border is solid; use solid or none)`);
      return [];
    }

    if (borderStyles.has(lower)) {
      style = lower;
    } else if (isWidth) {
      width = borderWidthKeywords.get(lower) ?? word;
    } else if (color === null) {
      color = word;
    } else {
      reader.expected("[width] [style] [color]");
      return [];
    }
  }

  if (style === null) {
    reader.expected("a style (solid or none)");
    return [];
  }

  const widths = sideNames.map(side => longhand(declaration, `border-${side}-width`, width));
  const rest = [longhand(declaration, "border-style", style)];
  const colors = color === null ? [] : [longhand(declaration, "border-color", color)];
  return [...widths, ...rest, ...colors];
}

function expandBackground(declaration: Declaration, reader: Reader): Declaration[] {
  const word = reader.single();
  if (word === null) {
    return [];
  }

  const isImageOrGradient = /^(url|image|image-set|(repeating-)?(linear|radial|conic)-gradient)\(/iu.test(word);
  if (isImageOrGradient) {
    reader.error(`${declaration.property} ${word}: images and gradients have no Style value in WinUI; declare a brush resource and use var(--its-name)`);
    return [];
  }

  const value = word.toLowerCase() === "none" ? "transparent" : word;
  return [longhand(declaration, "background-color", value)];
}

function expandFont(declaration: Declaration, reader: Reader): Declaration[] {
  const words = reader.words;
  const sizeIndex = words.findIndex(word => parseLength(splitSlash(word)[0] ?? "") !== null);
  if (sizeIndex === -1) {
    reader.expected("[style] [weight] size[/line-height] family");
    return [];
  }

  const result: Declaration[] = [];
  for (const word of words.slice(0, sizeIndex)) {
    const lower = word.toLowerCase();
    const isStyle = fontStyles.has(lower);
    const isWeight = fontWeightWords.has(lower) || /^\d+$/u.test(word);
    if (isStyle) {
      result.push(longhand(declaration, "font-style", lower));
    } else if (isWeight) {
      result.push(longhand(declaration, "font-weight", lower));
    }
  }

  const [size = "", lineHeight] = splitSlash(words[sizeIndex] ?? "");
  result.push(longhand(declaration, "font-size", size));

  if (lineHeight !== undefined) {
    result.push(longhand(declaration, "line-height", lineHeight));
  }

  const family = words.slice(sizeIndex + 1).join(" ");
  const hasFamily = family !== "";
  if (hasFamily) {
    result.push(longhand(declaration, "font-family", family));
  }

  return result;
}

function expandPair(declaration: Declaration, reader: Reader, names: readonly [string, string]): Declaration[] {
  const [first, second] = reader.words;
  const isValid = first !== undefined && reader.words.length <= 2;
  if (!isValid) {
    reader.expected("one or two values");
    return [];
  }

  return [longhand(declaration, names[0], first), longhand(declaration, names[1], second ?? first)];
}

// `2`, `span 2`, or `2 / 4`, `2 / span 2`: a start and an end line.
function expandLines(declaration: Declaration, reader: Reader, axis: "column" | "row"): Declaration[] {
  const [start, end, ...rest] = splitSlash(declaration.value);
  const isValid = start !== undefined && start !== "" && rest.length === 0;
  if (!isValid) {
    reader.expected("a line, `span n`, or `start / end`");
    return [];
  }

  const startDeclaration = longhand(declaration, `grid-${axis}-start`, start);
  return end === undefined ? [startDeclaration] : [startDeclaration, longhand(declaration, `grid-${axis}-end`, end)];
}

type Expansion = (declaration: Declaration, reader: Reader) => Declaration[];

const expansions = new Map<string, Expansion>([
  ["margin", (declaration, reader) => expandBox(declaration, sides, reader, name => `margin-${name}`)],
  ["padding", (declaration, reader) => expandBox(declaration, sides, reader, name => `padding-${name}`)],
  ["border-width", (declaration, reader) => expandBox(declaration, sides, reader, name => `border-${name}-width`)],
  [
    "border-radius",
    (declaration, reader) => {
      if (declaration.value.includes("/")) {
        reader.error("border-radius: elliptical corners (a / b) have no WinUI meaning; a CornerRadius is circular");
        return [];
      }

      return expandBox(declaration, corners, reader, name => `border-${name}-radius`);
    }
  ],
  ["border", (declaration, reader) => expandBorder(declaration, reader, sides)],
  ...sides.map((side): [string, Expansion] => [
    `border-${side}`,
    (declaration, reader) => expandBorder(declaration, reader, [side])
  ]),
  ["background", expandBackground],
  ["font", expandFont],
  ["gap", (declaration, reader) => expandPair(declaration, reader, ["row-gap", "column-gap"])],
  ["overflow", (declaration, reader) => expandPair(declaration, reader, ["overflow-x", "overflow-y"])],
  ["place-self", (declaration, reader) => expandPair(declaration, reader, ["align-self", "justify-self"])],
  ["grid-column", (declaration, reader) => expandLines(declaration, reader, "column")],
  ["grid-row", (declaration, reader) => expandLines(declaration, reader, "row")],
  [
    "flex-flow",
    (declaration, reader) => {
      const [direction = ""] = reader.words;
      return [longhand(declaration, "flex-direction", direction)];
    }
  ]
]);

// The shorthands whose whole value may be one var() to a WinUI Thickness or
// CornerRadius: not expanded.
const wholeVariableShorthands = new Set(["margin", "padding", "border-width", "border-radius"]);

export function expandDeclaration(declaration: Declaration, options: LowerOptions): Declaration[] {
  const expansion = expansions.get(declaration.property);
  if (expansion === undefined) {
    return [declaration];
  }

  const keepsVariable = wholeVariableShorthands.has(declaration.property) && isWholeVariable(declaration);
  if (keepsVariable) {
    return [declaration];
  }

  const reader = new Reader(declaration, options);
  return expansion(declaration, reader);
}
