import type { CardWording } from "../protocol.g.ts";
import type { Component, Snippet } from "svelte";

// card-frame.svelte's props, for TypeScript and for the component itself.
export interface CardFrameProps {
  wording: CardWording;
  note?: string;
  isShown?: boolean;
  end: Snippet;
  body?: Snippet | undefined;
}

declare const CardFrame: Component<CardFrameProps>;
// Imported as card-frame.svelte, which fallow does not trace to this file.
// fallow-ignore-next-line unused-export
export default CardFrame;
