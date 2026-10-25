import type { InjectView } from "../protocol.g.ts";
import { CardFrame } from "./card-frame.ts";
import { boxed } from "./layout.ts";
import { style, StyleKey } from "./styles.ts";
import type { Border } from "./winui.ts";
import { Button } from "./winui.ts";

// The Inject button, as a card under the Inject automatically switch. When it
// is shown, and whether it can be pressed, is the core's decision.
export class InjectCard {
  private readonly button = new Button();
  private readonly frame: CardFrame;
  private shown: InjectView;

  constructor(offer: InjectView, onClicked: () => void) {
    this.shown = offer;
    this.button.style = style(StyleKey.AccentButton);
    // Disabled at once, so a second click cannot queue a second injection.
    this.button.onClick(() => {
      this.button.isEnabled = false;
      onClicked();
    });
    this.frame = new CardFrame(offer.wording, this.button);
    this.update(offer);
  }

  get view(): Border {
    return this.frame.view;
  }

  update(offer: InjectView): void {
    this.shown = offer;
    this.frame.setWording(offer.wording);
    this.button.content = boxed(offer.button);
    this.button.isEnabled = offer.isEnabled;
  }

  // A failed injection renders no new view; the button the click disabled
  // comes back as the offer last shown.
  restore(): void {
    this.update(this.shown);
  }
}
