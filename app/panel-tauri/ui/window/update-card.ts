import type { UpdateView } from "../protocol.ts";
import type { Border } from "./winui.ts";
import { CardFrame } from "./card-frame.ts";
import { boxed } from "./layout.ts";
import { style, StyleKey } from "./styles.ts";
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
  private readonly frame: CardFrame;
  private shown: UpdateView | null = null;

  constructor(onClicked: () => void) {
    this.button.style = style(StyleKey.AccentButton);
    // Disabled at once, so a second click cannot start a second restart.
    this.button.onClick(() => {
      this.button.isEnabled = false;
      this.button.content = boxed(this.shown?.restarting ?? "");
      onClicked();
    });
    this.frame = new CardFrame(hiddenWording, this.button);
    this.frame.view.visibility = Visibility.Collapsed;
  }

  get view(): Border {
    return this.frame.view;
  }

  show(update: UpdateView | null): void {
    const isSame = JSON.stringify(update) === JSON.stringify(this.shown);
    if (isSame || update === null) {
      return;
    }
    this.shown = update;
    this.frame.setWording(update.wording);
    this.reset();
    this.frame.view.visibility = Visibility.Visible;
  }

  // The restart failed: the button comes back, so it can be tried again.
  reset(): void {
    this.button.content = boxed(this.shown?.button ?? "");
    this.button.isEnabled = true;
  }
}
