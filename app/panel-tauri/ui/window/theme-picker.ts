import type { ThemeOption } from "../protocol.g.ts";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import ThemePickerView from "./theme-picker.svelte";
import { ComboBox, projectAs } from "./winui.ts";

interface Props extends Record<string, unknown> {
  options: ThemeOption[];
}

// The theme for one Windows mode (theme-picker.svelte).
export class ThemePicker {
  readonly box: ComboBox;
  private readonly picker: Mounted<Props>;
  private options: ThemeOption[] = [];
  // So a refill is not taken for a choice.
  private isFilling = false;

  constructor(onPicked: (id: string) => void) {
    this.picker = mountControl<Props>(ThemePickerView, { options: [] });
    this.box = projectAs(this.picker.root, ComboBox);
    this.box.onSelectionChanged(() => {
      const option = this.options.at(this.box.selectedIndex);
      const isUserChoice = !this.isFilling && option !== undefined;
      if (isUserChoice) {
        onPicked(option.id);
      }
    });
  }

  // The items are rebuilt only when the options changed.
  fill(options: ThemeOption[], selectedId: string): void {
    this.isFilling = true;
    const isCurrent = JSON.stringify(this.options) === JSON.stringify(options);
    if (!isCurrent) {
      this.options = options;
      this.picker.set({ options });
    }

    this.select(selectedId);
    this.isFilling = false;
  }

  // A theme removed meanwhile falls back to the default theme, the first
  // option.
  private select(id: string): void {
    const wanted = Math.max(this.options.findIndex(option => option.id === id), 0);
    if (this.box.selectedIndex !== wanted) {
      this.box.selectedIndex = wanted;
    }
  }
}
