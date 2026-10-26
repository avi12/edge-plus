import { namedColors } from "./named-colors.ts";

// The values a declaration holds, read one token at a time. Each parser
// returns null when the token is not that kind of value, so a property can
// try the kinds it accepts and name the one it expected.

// WinUI measures in device-independent pixels, which is what CSS's px is. A
// rem is the browser's default root size; WinUI has no root font size.
const pixelsPerRem = 16;
const msPerSecond = 1000;
const maxChannel = 255;
const percentScale = 100;
const hueDegrees = 360;
const transparentColor = "#00FFFFFF";

type Depth = number;

function depthChange(character: string): Depth {
  if (character === "(") {
    return 1;
  }

  return character === ")" ? -1 : 0;
}

// Splits at `separator` outside parentheses and strings; empty pieces dropped
// when the separator is whitespace.
function splitOutside(text: string, isSeparator: (character: string) => boolean): string[] {
  const pieces: string[] = [];
  let depth = 0;
  let current = "";
  let quote = "";
  for (const character of text) {
    const isInString = quote !== "";
    if (isInString) {
      current += character;

      if (character === quote) {
        quote = "";
      }

      continue;
    }

    const isQuote = character === "\"" || character === "'";
    if (isQuote) {
      quote = character;
    }

    depth += depthChange(character);
    const isSplit = depth === 0 && !isQuote && isSeparator(character);
    if (isSplit) {
      pieces.push(current);
      current = "";
    } else {
      current += character;
    }
  }

  pieces.push(current);
  return pieces.map(piece => piece.trim());
}

export function splitWords(text: string): string[] {
  return splitOutside(text, character => character.trim() === "").filter(word => word !== "");
}

export function splitCommas(text: string): string[] {
  return splitOutside(text, character => character === ",");
}

export function splitSlash(text: string): string[] {
  return splitOutside(text, character => character === "/");
}

const lengthPattern = /^(-?(?:\d+\.?\d*|\.\d+))(px|rem)?$/iu;

// px, rem (1rem is 16px) or a bare 0, in device-independent pixels.
export function parseLength(token: string): number | null {
  const match = lengthPattern.exec(token);
  if (match === null) {
    return null;
  }

  const amount = Number(match[1]);
  const unit = (match[2] ?? "").toLowerCase();
  const isBare = unit === "";
  if (isBare) {
    return amount === 0 ? 0 : null;
  }

  return unit === "rem" ? amount * pixelsPerRem : amount;
}

export function parseNumber(token: string): number | null {
  const isNumber = /^-?(?:\d+\.?\d*|\.\d+)$/u.test(token);
  return isNumber ? Number(token) : null;
}

const percentPattern = /^(-?(?:\d+\.?\d*|\.\d+))%$/u;

// "50%" as 0.5.
export function parsePercent(token: string): number | null {
  const match = percentPattern.exec(token);
  return match === null ? null : Number(match[1]) / percentScale;
}

const timePattern = /^(\d+\.?\d*|\.\d+)(ms|s)$/iu;

export function parseMilliseconds(token: string): number | null {
  const match = timePattern.exec(token);
  if (match === null) {
    return null;
  }

  const amount = Number(match[1]);
  return (match[2] ?? "").toLowerCase() === "s" ? amount * msPerSecond : amount;
}

function hex(value: number): string {
  return Math.round(Math.min(maxChannel, Math.max(0, value))).toString(16).padStart(2, "0").toUpperCase();
}

// XAML spells a color #AARRGGBB; the alpha is dropped when it is opaque.
function xamlColor(red: number, green: number, blue: number, alpha: number): string {
  const isOpaque = alpha >= 1;
  const alphaPart = isOpaque ? "" : hex(alpha * maxChannel);
  return `#${alphaPart}${hex(red)}${hex(green)}${hex(blue)}`;
}

function parseHexColor(token: string): string | null {
  const match = /^#([0-9a-f]{3,8})$/iu.exec(token);
  const digits = match?.[1];
  if (digits === undefined) {
    return null;
  }

  const isShort = digits.length === 3 || digits.length === 4;
  const full = isShort ? digits.split("").map(digit => digit + digit).join("") : digits;
  const isValidLength = full.length === 6 || full.length === 8;
  if (!isValidLength) {
    return null;
  }

  // CSS writes alpha last (#RRGGBBAA), XAML first.
  const alphaPart = full.length === 8 ? full.slice(6) : "";
  return `#${alphaPart}${full.slice(0, 6)}`.toUpperCase();
}

function channelValue(part: string, scale: number): number | null {
  const percent = parsePercent(part);
  if (percent !== null) {
    return percent * scale;
  }

  return parseNumber(part);
}

function alphaValue(part: string | undefined): number | null {
  if (part === undefined) {
    return 1;
  }

  return channelValue(part, 1);
}

function parseRgbColor(token: string): string | null {
  const match = /^rgba?\((.*)\)$/iu.exec(token);
  if (match === null) {
    return null;
  }

  const [channels = "", alphaText] = (match[1] ?? "").includes("/")
    ? splitSlash(match[1] ?? "")
    : [match[1] ?? ""];
  const parts = channels.includes(",") ? splitCommas(channels) : splitWords(channels);
  const [redPart = "", greenPart = "", bluePart = "", legacyAlpha] = parts;
  const alpha = alphaValue(alphaText ?? legacyAlpha);
  const red = channelValue(redPart, maxChannel);
  const green = channelValue(greenPart, maxChannel);
  const blue = channelValue(bluePart, maxChannel);
  const isValid = red !== null && green !== null && blue !== null && alpha !== null && parts.length >= 3;
  return isValid ? xamlColor(red, green, blue, alpha) : null;
}

// The standard HSL to RGB conversion: `low` and `high` bound the channel, and
// the hue fraction picks its place on the ramp between them.
function hueChannel(low: number, high: number, hueFraction: number): number {
  const position = ((hueFraction % 1) + 1) % 1;
  const rampsUp = position < 1 / 6;
  const isAtHigh = position < 1 / 2;
  const rampsDown = position < 2 / 3;
  if (rampsUp) {
    return low + (high - low) * 6 * position;
  }

  if (isAtHigh) {
    return high;
  }

  return rampsDown ? low + (high - low) * (2 / 3 - position) * 6 : low;
}

function parseHslColor(token: string): string | null {
  const match = /^hsla?\((.*)\)$/iu.exec(token);
  if (match === null) {
    return null;
  }

  const [channels = "", alphaText] = splitSlash(match[1] ?? "");
  const parts = channels.includes(",") ? splitCommas(channels) : splitWords(channels);
  const [huePart = "", saturationPart = "", lightnessPart = "", legacyAlpha] = parts;
  const hue = parseNumber(huePart.replace(/deg$/iu, ""));
  const saturation = parsePercent(saturationPart);
  const lightness = parsePercent(lightnessPart);
  const alpha = alphaValue(alphaText ?? legacyAlpha);
  const isValid = hue !== null && saturation !== null && lightness !== null && alpha !== null;
  if (!isValid) {
    return null;
  }

  const isDark = lightness < 0.5;
  const high = isDark ? lightness * (1 + saturation) : lightness + saturation - lightness * saturation;
  const low = 2 * lightness - high;
  const hueFraction = (((hue % hueDegrees) + hueDegrees) % hueDegrees) / hueDegrees;
  const [red, green, blue] = [1 / 3, 0, -1 / 3].map(offset => hueChannel(low, high, hueFraction + offset) * maxChannel);
  return xamlColor(red ?? 0, green ?? 0, blue ?? 0, alpha);
}

// A color as XAML spells it, or null: hex (3, 4, 6 or 8 digits), rgb()/rgba(),
// hsl()/hsla(), a CSS name or `transparent`. `currentcolor` has no meaning in
// WinUI, where a Foreground is inherited by the control, not by a style.
export function parseColor(token: string): string | null {
  const lower = token.toLowerCase();
  if (lower === "transparent") {
    return transparentColor;
  }

  const named = namedColors.get(lower);
  if (named !== undefined) {
    return `#${named.toUpperCase()}`;
  }

  return parseHexColor(token) ?? parseRgbColor(token) ?? parseHslColor(token);
}

export interface VariableReference {
  // With the leading `--`.
  name: string;
  fallback: string | null;
}

export function parseVariable(token: string): VariableReference | null {
  const match = /^var\(\s*(--[\w-]+)\s*(?:,(.*))?\)$/isu.exec(token);
  const name = match?.[1];
  if (name === undefined) {
    return null;
  }

  const fallback = match?.[2]?.trim();
  return {
    name,
    fallback: fallback === undefined || fallback === "" ? null : fallback
  };
}
