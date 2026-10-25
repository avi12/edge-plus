// The plain values a component's props take, and how each is read: pure, so
// they are tested without WinUI. convert.ts turns them into the bindings'.

export interface ThicknessValue {
  left: number;
  top: number;
  right: number;
  bottom: number;
}

export interface CornerRadiusValue {
  topLeft: number;
  topRight: number;
  bottomRight: number;
  bottomLeft: number;
}

export interface ColorValue {
  a: number;
  r: number;
  g: number;
  b: number;
}

export const GridUnit = {
  Auto: "auto",
  Pixel: "pixel",
  Star: "star"
} as const;

export type GridUnit = (typeof GridUnit)[keyof typeof GridUnit];

export interface GridLengthParts {
  unit: GridUnit;
  value: number;
}

// 8 for every side, [8, 4] for left-right and top-bottom, [left, top, right,
// bottom] as XAML writes them, or a string: CSS order ("top right bottom
// left", 1 to 4 values, `px` allowed) or, with commas, XAML's.
export type ThicknessInput =
  | number
  | string
  | readonly [number, number]
  | readonly [number, number, number, number]
  | ThicknessValue;

// The same four-sided input for a corner radius, in the order XAML writes the
// corners: top left, top right, bottom right, bottom left.
export type CornerRadiusInput =
  | number
  | string
  | readonly [number, number]
  | readonly [number, number, number, number]
  | CornerRadiusValue;

// "#rgb", "#rrggbb", "#aarrggbb", "transparent", or WinUI's own value.
export type ColorInput = string | ColorValue;

// An object property such as a button's content takes any value; a string,
// number or boolean is boxed first.
export type ObjectInput = unknown;

const cssSeparator = /\s+/u;
const listSeparator = ",";
const pixelSuffix = "px";
const autoKeyword = "Auto";
const starSuffix = "*";
const maxChannel = 255;
const hexRadix = 16;
const shortHexLength = 3;
const opaque = maxChannel;
const colorPattern = /^#(?:[0-9a-f]{3}|[0-9a-f]{6}|[0-9a-f]{8})$/iu;
const transparentName = "transparent";

function toPixels(text: string): number {
  const trimmed = text.trim();
  const digits = trimmed.endsWith(pixelSuffix) ? trimmed.slice(0, -pixelSuffix.length) : trimmed;
  const value = Number(digits);
  const isLength = digits !== "" && !Number.isNaN(value);
  if (!isLength) {
    throw new TypeError(`"${text}" is not a length`);
  }

  return value;
}

// A list of 1 to 4 numbers as the four sides, in the order XAML writes them
// (left, top, right, bottom) or CSS does (top, right, bottom, left).
function sidesOf(values: readonly number[], order: "xaml" | "css"): [number, number, number, number] {
  const [first = 0, second = first, third, fourth] = values;
  if (order === "css") {
    const top = first;
    const right = second;
    return [fourth ?? right, top, right, third ?? top];
  }

  return [first, second, third ?? first, fourth ?? second];
}

function valuesOf(text: string): {
  values: number[];
  order: "xaml" | "css";
} {
  const isXaml = text.includes(listSeparator);
  const parts = isXaml ? text.split(listSeparator) : text.trim().split(cssSeparator);
  return {
    values: parts.map(toPixels),
    order: isXaml ? "xaml" : "css"
  };
}

// The four sides of a thickness input as [left, top, right, bottom].
export function thicknessSides(input: ThicknessInput): [number, number, number, number] {
  if (typeof input === "number") {
    return [input, input, input, input];
  }

  if (typeof input === "string") {
    const { values, order } = valuesOf(input);
    return sidesOf(values, order);
  }

  if ("left" in input) {
    return [input.left, input.top, input.right, input.bottom];
  }

  return sidesOf(input, "xaml");
}

// The four corners of a radius input as [topLeft, topRight, bottomRight,
// bottomLeft].
export function cornerSides(input: CornerRadiusInput): [number, number, number, number] {
  if (typeof input === "number") {
    return [input, input, input, input];
  }

  if (typeof input === "string") {
    const { values } = valuesOf(input);
    return sidesOf(values, "xaml");
  }

  if ("topLeft" in input) {
    return [input.topLeft, input.topRight, input.bottomRight, input.bottomLeft];
  }

  return sidesOf(input, "xaml");
}

// Pixels as a number, or "Auto", "*", "2*" or "120" as XAML writes them.
export function gridLengthParts(input: number | string): GridLengthParts {
  if (typeof input === "number") {
    return {
      unit: GridUnit.Pixel,
      value: input
    };
  }

  const text = input.trim();
  if (text === autoKeyword) {
    return {
      unit: GridUnit.Auto,
      value: 1
    };
  }

  if (text.endsWith(starSuffix)) {
    const weight = text.slice(0, -starSuffix.length);
    return {
      unit: GridUnit.Star,
      value: weight === "" ? 1 : toPixels(weight)
    };
  }

  return {
    unit: GridUnit.Pixel,
    value: toPixels(text)
  };
}

// The text of a color as WinUI's four channels, or null when it is not one
// (a theme resource name is then looked up).
export function parseColor(text: string): ColorValue | null {
  const name = text.trim().toLowerCase();
  if (name === transparentName) {
    return {
      a: 0,
      r: 0,
      g: 0,
      b: 0
    };
  }

  if (!colorPattern.test(name)) {
    return null;
  }

  const digits = name.slice(1);
  const isShort = digits.length === shortHexLength;
  const expanded = isShort ? digits.split("").map(digit => `${digit}${digit}`).join("") : digits;
  const hasAlpha = expanded.length > 6;
  const channels = [...expanded.matchAll(/../gu)].map(match => Number.parseInt(match[0], hexRadix));
  const [alpha = opaque, red = 0, green = 0, blue = 0] = hasAlpha ? channels : [opaque, ...channels];
  return {
    a: alpha,
    r: red,
    g: green,
    b: blue
  };
}

// [Owner, "Property", value]: Owner's static Set<Property> is called with the
// control and the value. A list of these sets several.
export type AttachedEntry = readonly [owner: object, property: string, value: unknown];
export type AttachedInput = AttachedEntry | readonly AttachedEntry[];

function isEntry(input: AttachedInput): input is AttachedEntry {
  return typeof input[1] === "string";
}

export function attachedList(input: AttachedInput): readonly AttachedEntry[] {
  return isEntry(input) ? [input] : input;
}
