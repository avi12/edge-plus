import { autoLength, starLength } from "./layout.ts";
import { style, StyleKey } from "./styles.ts";
import type { ComboBox, FrameworkElement } from "./winui.ts";
import {
  AutomationProperties,
  ColumnDefinition,
  Grid,
  TextBlock,
  VerticalAlignment
} from "./winui.ts";

const columnGap = 8;
// Wide enough for the longest choice a picker offers.
const pickerMinWidth = 200;

// `fill` takes the room; `end` keeps its own width at the row's end.
export function twoColumnRow(fill: FrameworkElement, end: FrameworkElement): Grid {
  const row = new Grid();
  row.columnSpacing = columnGap;
  for (const width of [starLength(), autoLength()]) {
    const column = new ColumnDefinition();
    column.width = width;
    row.columnDefinitions.append(column);
  }

  Grid.setColumn(end, 1);
  row.children.append(fill);
  row.children.append(end);
  return row;
}

export function pickerRow(label: string, picker: ComboBox): Grid {
  const text = new TextBlock();
  text.text = label;
  text.style = style(StyleKey.CardTitle);
  text.verticalAlignment = VerticalAlignment.Center;
  picker.minWidth = pickerMinWidth;
  AutomationProperties.setName(picker, label);
  return twoColumnRow(text, picker);
}
