import type { CardWording } from "../protocol.g.ts";
import type { FrameworkElement } from "./winui.ts";
import type { Component } from "svelte";

// feature-card.svelte's props, for TypeScript and for the component itself.
// onToggled hears only the user's flips; the switch then shows isOn as given,
// so whoever renders the card sets isOn to the flipped value.
export interface FeatureCardProps {
  wording: CardWording;
  isOn: boolean;
  isLocked: boolean;
  note: string;
  body?: FrameworkElement | undefined;
  onToggled: (isOn: boolean) => void;
}

declare const FeatureCard: Component<FeatureCardProps>;
// Imported as feature-card.svelte, which fallow does not trace to this file.
// fallow-ignore-next-line unused-export
export default FeatureCard;
