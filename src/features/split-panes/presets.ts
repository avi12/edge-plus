import type { Template } from "./layout";
import { Axis } from "./layout";

/** Persisted nowhere, but sent between the popup and the background */
export enum PresetId {
  columns2 = "columns2",
  rows2 = "rows2",
  columns3 = "columns3",
  mainAndStack = "mainAndStack",
  grid2x2 = "grid2x2",
  grid3x2 = "grid3x2"
}

const row = (...children: Template[]): Template => ({ axis: Axis.row, children });
const column = (...children: Template[]): Template => ({ axis: Axis.column, children });

/**
 * Slot 0 is the tab you are on; the other slots take the tabs to its right, in order, then new tab
 * pages. The popup draws each icon from the same template
 */
export const PRESETS: { id: PresetId; title: string; template: Template }[] = [
  { id: PresetId.columns2, title: "Side by side", template: row(0, 1) },
  { id: PresetId.rows2, title: "Stacked", template: column(0, 1) },
  { id: PresetId.columns3, title: "Three columns", template: row(0, 1, 2) },
  { id: PresetId.mainAndStack, title: "One big, two small", template: row(0, column(1, 2)) },
  { id: PresetId.grid2x2, title: "Two by two", template: row(column(0, 2), column(1, 3)) },
  { id: PresetId.grid3x2, title: "Three by two", template: column(row(0, 1, 2), row(3, 4, 5)) }
];
