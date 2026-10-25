import type { CardWording } from "../protocol.g.ts";
import { autoLength, glyph, starLength, thickness } from "./layout.ts";
import { style, StyleKey } from "./styles.ts";
import type { FrameworkElement } from "./winui.ts";
import {
  Border,
  ColumnDefinition,
  FontIcon,
  Grid,
  RowDefinition,
  StackPanel,
  TextBlock,
  VerticalAlignment,
  Visibility
} from "./winui.ts";

const iconSize = 20;
const iconGap = 16;
const bodyGap = 12;
// Between the words and the control, as in Windows Settings: without it a
// description wraps only at the control's own edge and runs under it.
const controlGap = 16;

// The Windows Settings card, built by hand from WinUI's own controls rather
// than a toolkit's SettingsCard (no UI library, to keep the supply chain small):
// an icon, a title over a description, and one control at the end. A feature
// card puts a switch there, the Inject card a button.
export class CardFrame {
  readonly view: Border;
  private readonly icon = new FontIcon();
  private readonly title = new TextBlock();
  private readonly description = new TextBlock();
  private readonly note = new TextBlock();
  private readonly layout = new Grid();

  constructor(wording: CardWording, control: FrameworkElement) {
    this.note.style = style(StyleKey.CardNote);
    this.note.visibility = Visibility.Collapsed;
    this.title.style = style(StyleKey.CardTitle);
    this.description.style = style(StyleKey.CardDescription);
    const text = new StackPanel();
    text.verticalAlignment = VerticalAlignment.Center;
    text.margin = thickness(0, 0, controlGap, 0);
    text.children.append(this.title);
    text.children.append(this.description);
    text.children.append(this.note);

    this.icon.fontSize = iconSize;
    this.icon.margin = thickness(0, 0, iconGap, 0);
    this.icon.verticalAlignment = VerticalAlignment.Center;
    control.verticalAlignment = VerticalAlignment.Center;

    for (const width of [autoLength(), starLength(), autoLength()]) {
      const column = new ColumnDefinition();
      column.width = width;
      this.layout.columnDefinitions.append(column);
    }
    const row = new RowDefinition();
    row.height = autoLength();
    this.layout.rowDefinitions.append(row);
    Grid.setColumn(text, 1);
    Grid.setColumn(control, 2);
    this.layout.children.append(this.icon);
    this.layout.children.append(text);
    this.layout.children.append(control);

    this.view = new Border();
    this.view.style = style(StyleKey.Card);
    this.view.child = this.layout;
    this.setWording(wording);
  }

  setWording(wording: CardWording): void {
    this.icon.glyph = glyph(wording.glyph);
    this.title.text = wording.title;
    this.description.text = wording.description;
  }

  setNote(note: string): void {
    this.note.text = note;
    this.note.visibility = note.length > 0 ? Visibility.Visible : Visibility.Collapsed;
  }

  setBody(body: FrameworkElement): void {
    const row = new RowDefinition();
    row.height = autoLength();
    this.layout.rowDefinitions.append(row);
    Grid.setRow(body, 1);
    Grid.setColumn(body, 1);
    Grid.setColumnSpan(body, 2);
    body.margin = thickness(0, bodyGap, 0, 0);
    this.layout.children.append(body);
  }
}
