import type { CardWording } from "../protocol.g.ts";
import CardFrame from "./card-frame.svelte";
import { boxed } from "./layout.ts";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import type { FrameworkElement } from "./winui.ts";
import { AutomationProperties, ToggleSwitch } from "./winui.ts";

// A card with a switch at its end.
export class FeatureCard {
  private readonly toggle = new ToggleSwitch();
  private readonly frame: Mounted<Record<string, unknown>>;
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
    this.frame = mountControl(CardFrame, {
      wording,
      control: this.toggle
    });
  }

  get view(): FrameworkElement {
    return this.frame.root;
  }

  update(isOn: boolean, isLocked: boolean, note: string): void {
    this.isUpdating = true;
    this.toggle.isOn = isOn;
    this.isUpdating = false;
    this.toggle.isEnabled = !isLocked;
    this.frame.set({ note });
  }

  setBody(body: FrameworkElement): void {
    this.frame.set({ body });
  }
}
