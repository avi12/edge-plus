// What the renderer imports from the CSS part (see API.md). The build side
// (translateStyleSheet, the Vite plugin) is imported from its own modules so
// the window's bundle does not carry it.

export { createCssNative, installCss } from "./runtime/native.ts";
export type { CssWinui } from "./runtime/native.ts";
export { installCssRuntime, registerStyleSheet, styledElement, unregisterStyleSheet } from "./runtime/runtime.ts";
export type { CssNative, StyledElement } from "./runtime/runtime.ts";
export type { StyleSheetData } from "./translate/types.ts";
