import { GridUnitType, PropertyValue } from "./winui.ts";

// WinRT's plain values, built the way the bindings take them.

// A string for an `object` property such as a button's content, which takes
// only a boxed value.
export function boxed(text: string): unknown {
  return PropertyValue.createString(text);
}

const ticksPerMillisecond = 10_000n;

export function thickness(left: number, top: number, right: number, bottom: number) {
  return {
    left,
    top,
    right,
    bottom
  };
}

export function uniform(size: number) {
  return thickness(size, size, size, size);
}

export function starLength(value = 1) {
  return {
    value,
    gridUnitType: GridUnitType.Star
  };
}

export function autoLength() {
  return {
    value: 1,
    gridUnitType: GridUnitType.Auto
  };
}

export function pixelLength(value: number) {
  return {
    value,
    gridUnitType: GridUnitType.Pixel
  };
}

export function milliseconds(count: number) {
  return { duration: BigInt(count) * ticksPerMillisecond };
}

// A Segoe Fluent Icons code point as the string FontIcon.glyph takes.
export function glyph(codePoint: number): string {
  return String.fromCodePoint(codePoint);
}
