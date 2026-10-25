import type { ThemeOption } from "../protocol.g.ts";
import { boxed } from "./layout.ts";
import { ComboBox, ComboBoxItem } from "./winui.ts";

// The theme for one Windows mode: Edge's default theme, then the user's
// themes, then Edge's own.
export class ThemePicker {
  readonly box = new ComboBox();
  private options: ThemeOption[] = [];
  // So a refill is not taken for a choice.
  private isFilling = false;

  constructor(onPicked: (id: string) => void) {
    this.box.onSelectionChanged(() => {
      const option = this.options.at(this.box.selectedIndex);
      const isUserChoice = !this.isFilling && option !== undefined;
      if (isUserChoice) {
        onPicked(option.id);
      }
    });
  }

  // Rebuilt only when the options changed, and then with new items: moving a
  // ComboBoxItem by removing and re-inserting the same element throws
  // (E_INVALIDARG).
  fill(options: ThemeOption[], selectedId: string): void {
    this.isFilling = true;
    const isCurrent = JSON.stringify(this.options) === JSON.stringify(options);
    if (!isCurrent) {
      this.rebuild(options);
    }

    this.select(selectedId);
    this.isFilling = false;
  }

  private rebuild(options: ThemeOption[]): void {
    this.box.items.clear();
    for (const option of options) {
      const item = new ComboBoxItem();
      item.content = boxed(option.label);
      this.box.items.append(item);
    }

    this.options = options;
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
