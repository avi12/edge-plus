import type { ThemeRow } from "../protocol.g.ts";
import { syncKeyed } from "./keyed-list.ts";
import * as motion from "./motion.ts";
import { emptyRow, keyText, rowKey, themeRow } from "./theme-row.ts";
import { FrameworkElement, projectAs, PropertyValue, StackPanel } from "./winui.ts";

const rowSpacing = 4;
const emptyRowKey = "empty";

// The themes installed in Edge that the user did not add here: listed, never
// reordered or removed.
export class EdgeThemeList {
  readonly view = new StackPanel();

  constructor() {
    this.view.spacing = rowSpacing;
    motion.animate(this.view, motion.addDelete);
  }

  // `empty` replaces the list (and the user's own) while there are no themes.
  update(rows: ThemeRow[], empty: string | null): void {
    const byKey = new Map(rows.map(row => [rowKey(row), row]));
    const order = [...byKey.keys()];
    if (empty !== null) {
      order.push(emptyRowKey);
    }

    syncKeyed({
      items: this.view.children,
      keyOf: element => keyText(projectAs(element, FrameworkElement).tag),
      create(key) {
        const row = byKey.get(key);
        const element = row === undefined ? emptyRow(empty ?? "") : themeRow(row, null);
        element.tag = PropertyValue.createString(key);
        return element;
      }
    }, order);
  }
}
