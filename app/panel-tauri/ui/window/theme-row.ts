import type { ThemeRow } from "../protocol.g.ts";
import {
  autoLength,
  glyph,
  pixelLength,
  starLength,
  thickness
} from "./layout.ts";
import { style, StyleKey } from "./styles.ts";
import {
  AutomationProperties,
  Border,
  Button,
  ColumnDefinition,
  FontIcon,
  Grid,
  Orientation,
  StackPanel,
  TextBlock,
  TextTrimming,
  VerticalAlignment
} from "./winui.ts";
import { unboxObject } from "@microsoft/dynwinrt";

const columnGap = 8;
const removeGlyph = 0xE711;
const removeGlyphSize = 12;
const gripGlyph = 0xE76F;
const gripGlyphSize = 12;
const gripWidth = 16;
const shown = 1;
const hidden = 0;
// Inside a row, so the list's rows and Edge's rows line up.
const rowInset = 4;
const rowKeySeparator = "\n";

// Keyed by everything a row shows, so an unchanged row is left alone (and
// does not re-animate).
export function rowKey(row: ThemeRow): string {
  return [row.id, row.name, row.badges.join(",")].join(rowKeySeparator);
}

export function idOf(key: string): string {
  return key.split(rowKeySeparator)[0];
}

export function nameOf(key: string): string {
  return key.split(rowKeySeparator)[1];
}

// A row key as a list holds it: a boxed string.
export function keyText(item: unknown): string {
  const value = unboxObject(item);
  return typeof value === "string" ? value : "";
}

// One theme: a grip, its name, its badges and a remove button. `onRemove` is
// null for a theme installed in Edge, whose grip and remove button are only
// there invisibly, so every row's columns line up.
export function themeRow(theme: ThemeRow, onRemove: (() => void) | null): Grid {
  const isOwn = onRemove !== null;
  const row = new Grid();
  row.columnSpacing = columnGap;
  row.padding = thickness(rowInset, 0, rowInset, 0);
  for (const width of [pixelLength(gripWidth), starLength(), autoLength(), autoLength()]) {
    const column = new ColumnDefinition();
    column.width = width;
    row.columnDefinitions.append(column);
  }

  AutomationProperties.setName(row, theme.name);
  const grip = icon(gripGlyph, gripGlyphSize);
  grip.opacity = isOwn ? shown : hidden;
  row.children.append(grip);
  const name = title(theme.name);
  Grid.setColumn(name, 1);
  row.children.append(name);
  const badgeList = badges(theme.badges);
  Grid.setColumn(badgeList, 2);
  row.children.append(badgeList);
  const remove = removeButton(theme.removeLabel, onRemove);
  Grid.setColumn(remove, 3);
  row.children.append(remove);
  return row;
}

// Shown in place of the lists while there are no themes.
export function emptyRow(text: string): TextBlock {
  const empty = new TextBlock();
  empty.text = text;
  empty.style = style(StyleKey.CardDescription);
  return empty;
}

function icon(codePoint: number, size: number): FontIcon {
  const font = new FontIcon();
  font.glyph = glyph(codePoint);
  font.fontSize = size;
  return font;
}

function title(name: string): TextBlock {
  const text = new TextBlock();
  text.text = name;
  text.style = style(StyleKey.CardTitle);
  text.verticalAlignment = VerticalAlignment.Center;
  text.textTrimming = TextTrimming.CharacterEllipsis;
  return text;
}

function badges(names: string[]): StackPanel {
  const list = new StackPanel();
  list.orientation = Orientation.Horizontal;
  list.spacing = columnGap / 2;
  for (const name of names) {
    const text = new TextBlock();
    text.text = name;
    text.style = style(StyleKey.BadgeText);
    const frame = new Border();
    frame.style = style(StyleKey.Badge);
    frame.child = text;
    list.children.append(frame);
  }

  return list;
}

function removeButton(label: string, onRemove: (() => void) | null): Button {
  const remove = new Button();
  remove.content = icon(removeGlyph, removeGlyphSize);
  AutomationProperties.setName(remove, label);

  if (onRemove === null) {
    remove.isEnabled = false;
    remove.opacity = hidden;
    return remove;
  }

  remove.onClick(onRemove);
  return remove;
}
