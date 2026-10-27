import type { UIElement } from "../winui.ts";
import { getContext, setContext } from "svelte";
import type { ElementRef } from "winui-svelte";

// A card's title, held for the control at the card's end to name: a screen
// reader reads the switch or button by the title's own text
// (automationLabeledBy), so the title is written once, in the markup.

export type CardLabel = ElementRef<UIElement>;

const cardLabelContext = Symbol("card label");

export function provideCardLabel(label: CardLabel): void {
  setContext(cardLabelContext, label);
}

export function useCardLabel(): CardLabel {
  const label = getContext<CardLabel | undefined>(cardLabelContext);
  if (label === undefined) {
    throw new Error("a card title is only written inside a card");
  }

  return label;
}
