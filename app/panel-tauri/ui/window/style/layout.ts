// WinRT's plain values, built the way the bindings take them. The ones every
// WinUI app needs are svelte-winui's.

const ticksPerMillisecond = 10_000n;

export function milliseconds(count: number) {
  return { duration: BigInt(count) * ticksPerMillisecond };
}

// A Segoe Fluent Icons code point as the string FontIcon.glyph takes.
export function glyph(codePoint: number): string {
  return String.fromCodePoint(codePoint);
}
