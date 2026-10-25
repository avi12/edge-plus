import type { CardWording } from "../protocol.ts";
import type { Border, FrameworkElement } from "./winui.ts";
import { CardFrame } from "./card-frame.ts";
import { boxed } from "./layout.ts";
import { AutomationProperties, ToggleSwitch } from "./winui.ts";

// A card with a switch at its end.
export class FeatureCard {
  private readonly toggle = new ToggleSwitch();
  private readonly frame: CardFrame;
  // So update's write to the switch is not taken for a click.
  private isUpdating = false;

  constructor(wording: CardWording, onToggled: (isOn: boolean) => void) {
    this.toggle.onContent = boxed("");
    this.toggle.offContent = boxed("");
    this.toggle.minWidth = 0;
    AutomationProperties.setName(this.toggle, wording.title);
    this.toggle.onToggled(() => {
      if (!this.isUpdating) {
        onToggled(this.toggle.isOn);
      }
    });
    this.frame = new CardFrame(wording, this.toggle);
  }

  get view(): Border {
    return this.frame.view;
  }

  update(isOn: boolean, isLocked: boolean, note: string): void {
    this.isUpdating = true;
    this.toggle.isOn = isOn;
    this.isUpdating = false;
    this.toggle.isEnabled = !isLocked;
    this.frame.setNote(note);
  }

  setBody(body: FrameworkElement): void {
    this.frame.setBody(body);
  }
}
