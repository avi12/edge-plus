import type { UpdateView } from "../protocol.g.ts";
import CardFrame from "./card-frame.svelte";
import { boxed } from "./layout.ts";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import { style, StyleKey } from "./styles.ts";
import type { FrameworkElement } from "./winui.ts";
import { Button, Visibility } from "./winui.ts";

const hiddenWording = {
  title: "",
  description: "",
  glyph: 0
};

// A release that is downloaded and checked, with the button that restarts Edge+
// into it. Hidden until one is ready.
export class UpdateCard {
  private readonly button = new Button();
  private readonly frame: Mounted<Record<string, unknown>>;
  private shown: UpdateView | null = null;

  constructor(onClicked: () => void) {
    this.button.style = style(StyleKey.AccentButton);
    // Disabled at once, so a second click cannot start a second restart.
    this.button.onClick(() => {
      this.button.isEnabled = false;
      this.button.content = boxed(this.shown?.restarting ?? "");
      onClicked();
    });
    this.frame = mountControl(CardFrame, {
      wording: hiddenWording,
      control: this.button
    });
    this.frame.root.visibility = Visibility.Collapsed;
  }

  get view(): FrameworkElement {
    return this.frame.root;
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
    this.frame.set({ wording: update.wording });
    this.reset();
    this.frame.root.visibility = Visibility.Visible;
  }

  // The restart failed: the button comes back, so it can be tried again.
  reset(): void {
    this.button.content = boxed(this.shown?.button ?? "");
    this.button.isEnabled = true;
  }
}
