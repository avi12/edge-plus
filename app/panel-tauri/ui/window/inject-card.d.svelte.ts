import type { InjectView } from "../protocol.g.ts";
import type { Component } from "svelte";

// inject-card.svelte's props, for TypeScript and for the component itself.
export interface InjectCardProps {
  offer: InjectView;
  // Pressed and not yet answered: the button stays disabled.
  isPressed: boolean;
  onClicked: () => void;
}

declare const InjectCard: Component<InjectCardProps>;
// Imported as inject-card.svelte, which fallow does not trace to this file.
// fallow-ignore-next-line unused-export
export default InjectCard;
