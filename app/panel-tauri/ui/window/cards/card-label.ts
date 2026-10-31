import { getContext, setContext } from "svelte";
import { elementRef } from "winui-svelte";
import type { ElementRef } from "winui-svelte";
import type { UIElement } from "winui-svelte/bindings/microsoft/ui/xaml/UIElement";

export type CardLabel = ElementRef<UIElement>;

const cardLabelContext = Symbol("card label");

/** Names the title below by this reference, for the control at the card's end. */
export function provideCardLabel(label: CardLabel) {
  setContext(cardLabelContext, label);
}

/** The reference a title writes itself into; a title outside a card names nothing. */
export function useCardLabel() {
  return getContext<CardLabel | undefined>(cardLabelContext) ?? elementRef<UIElement>();
}
