import type { ComboBox } from "./winui.ts";
import { autoLength, starLength } from "./layout.ts";
import { style, StyleKey } from "./styles.ts";
import { AutomationProperties, ColumnDefinition, Grid, TextBlock, VerticalAlignment } from "./winui.ts";

const columnGap = 8;
// Wide enough for the longest choice a picker offers.
const pickerMinWidth = 200;

export function pickerRow(label: string, picker: ComboBox): Grid {
  const row = new Grid();
  row.columnSpacing = columnGap;
  for (const width of [starLength(), autoLength()]) {
    const column = new ColumnDefinition();
    column.width = width;
    row.columnDefinitions.append(column);
  }
  const text = new TextBlock();
  text.text = label;
  text.style = style(StyleKey.CardTitle);
  text.verticalAlignment = VerticalAlignment.Center;
  row.children.append(text);
  picker.minWidth = pickerMinWidth;
  AutomationProperties.setName(picker, label);
  Grid.setColumn(picker, 1);
  row.children.append(picker);
  return row;
}
