import type { InjectView } from "../protocol.g.ts";
import type { InjectCardProps } from "./inject-card.d.svelte.ts";
import InjectCardView from "./inject-card.svelte";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import type { FrameworkElement } from "./winui.ts";

// The Inject card (inject-card.svelte), for the page to update from each view.
export class InjectCard {
  private readonly card: Mounted<InjectCardProps>;

  constructor(offer: InjectView, onClicked: () => void) {
    this.card = mountControl(InjectCardView, {
      offer,
      isPressed: false,
      // Disabled at once, so a second click cannot queue a second injection.
      onClicked: () => {
        this.card.set({ isPressed: true });
        onClicked();
      }
    });
  }

  get view(): FrameworkElement {
    return this.card.root;
  }

  update(offer: InjectView): void {
    this.card.set({
      offer,
      isPressed: false
    });
  }

  // A failed injection renders no new view; the button the click disabled
  // comes back as the offer last shown.
  restore(): void {
    this.card.set({ isPressed: false });
  }
}
