import type { UpdateView } from "../protocol.g.ts";
import type { Component } from "svelte";

// update-card.svelte's props, for TypeScript and for the component itself.
export interface UpdateCardProps {
  // null until a release is ready, which keeps the card hidden.
  update: UpdateView | null;
  // Pressed and not yet answered: the button says it is restarting.
  isPressed: boolean;
  onClicked: () => void;
}

declare const UpdateCard: Component<UpdateCardProps>;
// Imported as update-card.svelte, which fallow does not trace to this file.
// fallow-ignore-next-line unused-export
export default UpdateCard;
