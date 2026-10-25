import type { UpdateView } from "../protocol.g.ts";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import type { UpdateCardProps } from "./update-card.d.svelte.ts";
import UpdateCardView from "./update-card.svelte";
import type { FrameworkElement } from "./winui.ts";

// The update card (update-card.svelte), for the page to show once a release
// is ready.
export class UpdateCard {
  private readonly card: Mounted<UpdateCardProps>;
  private shown: UpdateView | null = null;

  constructor(onClicked: () => void) {
    this.card = mountControl(UpdateCardView, {
      update: null,
      isPressed: false,
      // Disabled at once, so a second click cannot start a second restart.
      onClicked: () => {
        this.card.set({ isPressed: true });
        onClicked();
      }
    });
  }

  get view(): FrameworkElement {
    return this.card.root;
  }

  show(update: UpdateView | null): void {
    if (update === null) {
      return;
    }

    const isShownAlready = JSON.stringify(update) === JSON.stringify(this.shown);
    if (isShownAlready) {
      return;
    }

    this.shown = update;
    this.card.set({
      update,
      isPressed: false
    });
  }

  // The restart failed: the button comes back, so it can be tried again.
  reset(): void {
    this.card.set({ isPressed: false });
  }
}
