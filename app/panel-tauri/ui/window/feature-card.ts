import type { CardWording } from "../protocol.g.ts";
import type { FeatureCardProps } from "./feature-card.d.svelte.ts";
import FeatureCardView from "./feature-card.svelte";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import type { FrameworkElement } from "./winui.ts";

// A card with a switch at its end (feature-card.svelte), for the page to
// update from each view.
export class FeatureCard {
  private readonly card: Mounted<FeatureCardProps>;

  constructor(wording: CardWording, onToggled: (isOn: boolean) => void) {
    this.card = mountControl(FeatureCardView, {
      wording,
      isOn: false,
      isLocked: false,
      note: "",
      onToggled: isOn => {
        this.card.set({ isOn });
        onToggled(isOn);
      }
    });
  }

  get view(): FrameworkElement {
    return this.card.root;
  }

  update(isOn: boolean, isLocked: boolean, note: string): void {
    this.card.set({
      isOn,
      isLocked,
      note
    });
  }

  setBody(body: FrameworkElement): void {
    this.card.set({ body });
  }
}
