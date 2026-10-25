import type { Action, CardView, ThemeOption, ThemeRow, ThemingView } from "../protocol.ts";
import { DynWinRtType, DynWinRtValue, unboxObject } from "@microsoft/dynwinrt";
import { pickerRow } from "./card-rows.ts";
import { FeatureCard } from "./feature-card.ts";
import { syncKeyed } from "./keyed-list.ts";
import { autoLength, boxed, glyph, pixelLength, starLength, thickness } from "./layout.ts";
import * as motion from "./motion.ts";
import { style, StyleKey } from "./styles.ts";
import {
  AutomationProperties,
  Border,
  Button,
  ColumnDefinition,
  ComboBox,
  ComboBoxItem,
  ContentControl,
  DataTemplate,
  DependencyObject,
  FocusState,
  FrameworkElement,
  FontIcon,
  Grid,
  IVector_Object,
  KeyRoutedEventArgs,
  ListView,
  ListViewItem,
  ListViewSelectionMode,
  Orientation,
  projectAs,
  PropertyValue,
  StackPanel,
  TextBlock,
  TextBox,
  TextTrimming,
  VerticalAlignment,
  VirtualKey,
  Visibility,
  VisualStateManager,
  VisualTreeHelper,
  XamlReader
} from "./winui.ts";

// A .crx or .zip file, or an unpacked folder.
export const LocalThemeKind = {
  File: "file",
  Folder: "folder"
} as const;
export type LocalThemeKind = (typeof LocalThemeKind)[keyof typeof LocalThemeKind];

// Runs an action in the core; resolves to why it failed, or null.
export type RunAction = (action: Action) => Promise<string | null>;

// Resolves to null when the user cancels.
export type PickLocalTheme = (kind: LocalThemeKind) => Promise<string | null>;

const rowSpacing = 8;
const columnGap = 8;
const dimmedOpacity = 0.5;
const removeGlyph = 0xE711;
const removeGlyphSize = 12;
const gripGlyph = 0xE76F;
const gripGlyphSize = 12;
const gripWidth = 16;
// Inside a row, so the list's rows and Edge's rows line up.
const rowInset = 4;
const normalState = "Normal";
const emptyRowKey = "empty";
const rowKeySeparator = "\n";
// Parsed at run time: the slot each own-theme row is put in.
const rowSlotTemplate =
  "<DataTemplate xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">" +
  "<ContentControl IsTabStop=\"False\" HorizontalContentAlignment=\"Stretch\" " +
  "HorizontalAlignment=\"Stretch\"/></DataTemplate>";

// Keyed by everything a row shows, so an unchanged row is left alone (and
// does not re-animate).
function rowKey(row: ThemeRow): string {
  return [row.id, row.name, row.badges.join(",")].join(rowKeySeparator);
}

function idOf(key: string): string {
  return key.split(rowKeySeparator)[0];
}

function nameOf(key: string): string {
  return key.split(rowKeySeparator)[1];
}

function textOf(item: unknown): string {
  const value = unboxObject(item);
  return typeof value === "string" ? value : "";
}

// Negative `offset` is up; stops at either end.
export function moved(ids: readonly string[], id: string, offset: number): string[] {
  const from = ids.indexOf(id);
  if (from < 0) {
    return [...ids];
  }
  const to = Math.min(Math.max(from + offset, 0), ids.length - 1);
  const order = [...ids];
  order.splice(from, 1);
  order.splice(to, 0, id);
  return order;
}

export class ThemingCard {
  private readonly card: FeatureCard;
  private readonly body = new StackPanel();
  private readonly storeLink = new TextBox();
  private readonly add = new Button();
  private readonly pickFile = new Button();
  private readonly pickFolder = new Button();
  private readonly addLocal = new TextBlock();
  private readonly error = new TextBlock();
  private readonly lightPicker = new ComboBox();
  private readonly darkPicker = new ComboBox();
  // No selection: with it on, each dragged row kept the accent bar, so two
  // drags looked like two selected themes. Alt+Up / Alt+Down act on the
  // focused row instead.
  private readonly ownThemes = new ListView();
  // The list's items are the rows' keys, not the rows: the built-in reorder
  // only commits a dragged move to a list of plain items. Each container shown
  // gets a freshly built row (fillContainer), so a row is never moved between
  // recycled containers.
  private readonly ownKeys = IVector_Object.from(DynWinRtValue.createVector([], DynWinRtType.object()));
  private ownRows = new Map<string, ThemeRow>();
  private readonly edgeThemes = new StackPanel();
  private readonly pickerOptions = new Map<ComboBox, ThemeOption[]>();
  // So a refill of the pickers is not taken for a choice.
  private isUpdating = false;
  private readonly run: RunAction;
  private readonly pick: PickLocalTheme;

  // `view` is the theming card's first view, which carries its labels.
  constructor(view: CardView, theming: ThemingView, run: RunAction, pick: PickLocalTheme) {
    this.run = run;
    this.pick = pick;
    this.card = new FeatureCard(view.wording, isOn => {
      this.runShowingError({
        kind: "setSwitch",
        key: view.key,
        isOn
      });
    });
    this.body.spacing = rowSpacing;
    this.edgeThemes.spacing = rowSpacing / 2;
    this.add.style = style(StyleKey.AccentButton);
    this.add.onClick(() => this.addStore());
    this.storeLink.onKeyDown((...args: unknown[]) => {
      const key = projectAs(args[1], KeyRoutedEventArgs);
      if (key.key === VirtualKey.Enter) {
        this.addStore();
      }
    });
    this.pickFile.onClick(() => this.addLocalTheme(LocalThemeKind.File));
    this.pickFolder.onClick(() => this.addLocalTheme(LocalThemeKind.Folder));
    this.lightPicker.onSelectionChanged(() => this.onPicked("light", this.lightPicker));
    this.darkPicker.onSelectionChanged(() => this.onPicked("dark", this.darkPicker));
    this.error.style = style(StyleKey.CardError);
    this.error.visibility = Visibility.Collapsed;

    this.ownThemes.selectionMode = ListViewSelectionMode.None;
    this.ownThemes.isItemClickEnabled = false;
    this.ownThemes.canReorderItems = true;
    this.ownThemes.canDragItems = true;
    this.ownThemes.allowDrop = true;
    this.ownThemes.itemContainerStyle = style(StyleKey.ThemeListItem);
    this.ownThemes.itemTemplate = projectAs(XamlReader.load(rowSlotTemplate), DataTemplate);
    this.ownThemes.itemsSource = this.ownKeys;
    this.ownThemes.onContainerContentChanging((_sender, args) => {
      const key = textOf(args.item);
      const row = this.ownRows.get(key);
      const hasRow = !args.inRecycleQueue && row !== undefined;
      if (!hasRow) {
        return;
      }
      const slot = projectAs(args.itemContainer.contentTemplateRoot, ContentControl);
      slot.content = this.themeRow(row, true);
      AutomationProperties.setName(args.itemContainer, nameOf(key));
    });
    this.ownThemes.onDragItemsCompleted(() => {
      // WinUI can leave the dragged (or last pressed) row in its Pressed or
      // PointerOver look after a drop; every row goes back to Normal.
      this.resetRowStates();
      this.reorder(this.currentOrder());
    });
    // Preview: the list's items take arrow keys for moving focus before a
    // bubbling KeyDown would see them.
    this.ownThemes.onPreviewKeyDown((...args: unknown[]) => this.onThemeListKeyDown(projectAs(args[1], KeyRoutedEventArgs)));

    const labels = theming.labels;
    this.storeLink.placeholderText = labels.storeLinkPlaceholder;
    this.add.content = boxed(labels.add);
    this.addLocal.text = labels.addLocal;
    this.pickFile.content = boxed(labels.pickFile);
    this.pickFolder.content = boxed(labels.pickFolder);
    AutomationProperties.setName(this.lightPicker, labels.whenLight);
    AutomationProperties.setName(this.darkPicker, labels.whenDark);
    AutomationProperties.setName(this.ownThemes, labels.themeListName);
    AutomationProperties.setHelpText(this.ownThemes, labels.reorderHelp);

    this.body.children.append(this.storeRow());
    this.body.children.append(this.localRow());
    this.body.children.append(this.error);
    this.body.children.append(pickerRow(labels.whenLight, this.lightPicker));
    this.body.children.append(pickerRow(labels.whenDark, this.darkPicker));
    this.body.children.append(this.ownThemes);
    this.body.children.append(this.edgeThemes);
    this.card.setBody(this.body);
    motion.animateItems(this.ownThemes, motion.listItems);
    motion.animate(this.edgeThemes, motion.addDelete);
    motion.animate(this.body, motion.reposition);
  }

  get view(): Border {
    return this.card.view;
  }

  update(view: CardView): void {
    const theming = view.theming;
    if (theming === null) {
      return;
    }
    this.card.update(view.isOn, view.isLocked, view.note);
    this.body.opacity = view.isOn ? 1 : dimmedOpacity;
    this.isUpdating = true;
    this.fill(this.lightPicker, theming.options, theming.light);
    this.fill(this.darkPicker, theming.options, theming.dark);
    this.isUpdating = false;

    this.ownRows = new Map(theming.own.map(row => [rowKey(row), row]));
    const ownOrder = [...this.ownRows.keys()];
    syncKeyed(
      {
        size: this.ownKeys.size,
        getAt: index => textOf(this.ownKeys.getAt(index)),
        removeAt: index => this.ownKeys.removeAt(index),
        insertAt: (index, key) => this.ownKeys.insertAt(index, PropertyValue.createString(key))
      },
      ownOrder,
      key => key,
      key => key
    );
    const edgeRows = new Map(theming.edge.map(row => [rowKey(row), row]));
    const edgeOrder = [...edgeRows.keys()];
    if (theming.empty !== null) {
      edgeOrder.push(emptyRowKey);
    }
    syncKeyed(
      this.edgeThemes.children,
      edgeOrder,
      element => textOf(projectAs(element, FrameworkElement).tag),
      key => {
        const row = edgeRows.get(key);
        const element = row === undefined ? this.emptyRow(theming.empty ?? "") : this.themeRow(row, false);
        element.tag = PropertyValue.createString(key);
        return element;
      }
    );
    this.resetRowStates();
    this.ownThemes.visibility = ownOrder.length > 0 ? Visibility.Visible : Visibility.Collapsed;
  }

  private currentOrder(): string[] {
    const order: string[] = [];
    for (let i = 0; i < this.ownKeys.size; i++) {
      order.push(idOf(textOf(this.ownKeys.getAt(i))));
    }
    return order;
  }

  // Alt+Up / Alt+Down: a reorder for whoever does not drag.
  private onThemeListKeyDown(event: KeyRoutedEventArgs): void {
    const isMoveKey = event.key === VirtualKey.Up || event.key === VirtualKey.Down;
    const isMove = isMoveKey && event.keyStatus.isMenuKeyDown;
    if (!isMove) {
      return;
    }
    const index = this.focusedRowIndex(event.originalSource);
    if (index < 0) {
      return;
    }
    event.handled = true;
    const id = idOf(textOf(this.ownKeys.getAt(index)));
    const offset = event.key === VirtualKey.Up ? -1 : 1;
    void this.moveKeepingFocus(moved(this.currentOrder(), id, offset), id);
  }

  private resetRowStates(): void {
    for (let i = 0; i < this.ownKeys.size; i++) {
      const container = this.ownThemes.containerFromIndex(i);
      if (container !== null) {
        VisualStateManager.goToState(projectAs(container, ListViewItem), normalState, false);
      }
    }
  }

  // Focus is on the list item itself or on its remove button.
  private focusedRowIndex(focused: unknown): number {
    let element: DependencyObject | null = focused === null ? null : projectAs(focused, DependencyObject);
    while (element !== null) {
      const index = this.ownThemes.indexFromContainer(element);
      if (index >= 0) {
        return index;
      }
      element = VisualTreeHelper.getParent(element);
    }
    return -1;
  }

  private async moveKeepingFocus(order: string[], id: string): Promise<void> {
    await this.reorder(order);
    const index = this.currentOrder().indexOf(id);
    if (index < 0) {
      return;
    }
    const container = this.ownThemes.containerFromIndex(index);
    if (container !== null) {
      projectAs(container, ListViewItem).focus(FocusState.Keyboard);
    }
  }

  private storeRow(): Grid {
    const row = new Grid();
    row.columnSpacing = columnGap;
    for (const width of [starLength(), autoLength()]) {
      const column = new ColumnDefinition();
      column.width = width;
      row.columnDefinitions.append(column);
    }
    Grid.setColumn(this.add, 1);
    row.children.append(this.storeLink);
    row.children.append(this.add);
    return row;
  }

  private localRow(): StackPanel {
    const row = new StackPanel();
    row.orientation = Orientation.Horizontal;
    row.spacing = columnGap;
    this.addLocal.style = style(StyleKey.CardDescription);
    this.addLocal.verticalAlignment = VerticalAlignment.Center;
    row.children.append(this.addLocal);
    row.children.append(this.pickFile);
    row.children.append(this.pickFolder);
    return row;
  }

  private emptyRow(text: string): TextBlock {
    const empty = new TextBlock();
    empty.text = text;
    empty.style = style(StyleKey.CardDescription);
    return empty;
  }

  // Edge's own themes get an invisible grip and remove button, so every row's
  // columns line up.
  private themeRow(theme: ThemeRow, isOwn: boolean): FrameworkElement {
    const row = new Grid();
    row.columnSpacing = columnGap;
    row.padding = thickness(rowInset, 0, rowInset, 0);
    for (const width of [pixelLength(gripWidth), starLength(), autoLength(), autoLength()]) {
      const column = new ColumnDefinition();
      column.width = width;
      row.columnDefinitions.append(column);
    }
    AutomationProperties.setName(row, theme.name);
    const grip = new FontIcon();
    grip.glyph = glyph(gripGlyph);
    grip.fontSize = gripGlyphSize;
    grip.opacity = isOwn ? 1 : 0;
    row.children.append(grip);

    const title = new TextBlock();
    title.text = theme.name;
    title.style = style(StyleKey.CardTitle);
    title.verticalAlignment = VerticalAlignment.Center;
    title.textTrimming = TextTrimming.CharacterEllipsis;
    Grid.setColumn(title, 1);
    row.children.append(title);

    const badges = new StackPanel();
    badges.orientation = Orientation.Horizontal;
    badges.spacing = columnGap / 2;
    for (const badge of theme.badges) {
      const text = new TextBlock();
      text.text = badge;
      text.style = style(StyleKey.BadgeText);
      const frame = new Border();
      frame.style = style(StyleKey.Badge);
      frame.child = text;
      badges.children.append(frame);
    }
    Grid.setColumn(badges, 2);
    row.children.append(badges);

    const icon = new FontIcon();
    icon.glyph = glyph(removeGlyph);
    icon.fontSize = removeGlyphSize;
    const remove = new Button();
    remove.content = icon;
    remove.isEnabled = isOwn;
    remove.opacity = isOwn ? 1 : 0;
    AutomationProperties.setName(remove, theme.removeLabel);
    remove.onClick(() => this.runShowingError({
      kind: "removeTheme",
      id: theme.id
    }));
    Grid.setColumn(remove, 3);
    row.children.append(remove);
    return row;
  }

  // Rebuilt only when the options changed, and then with new items: moving a
  // ComboBoxItem by removing and re-inserting the same element throws
  // (E_INVALIDARG). Runs under isUpdating, so the re-selection re-applies no
  // theme.
  private fill(picker: ComboBox, options: ThemeOption[], selectedId: string): void {
    const shown = this.pickerOptions.get(picker) ?? [];
    const isCurrent = JSON.stringify(shown) === JSON.stringify(options);
    if (!isCurrent) {
      picker.items.clear();
      for (const option of options) {
        const item = new ComboBoxItem();
        item.content = boxed(option.label);
        picker.items.append(item);
      }
      this.pickerOptions.set(picker, options);
    }
    // A theme removed meanwhile falls back to the default theme, the first option.
    const wanted = Math.max(options.findIndex(option => option.id === selectedId), 0);
    if (picker.selectedIndex !== wanted) {
      picker.selectedIndex = wanted;
    }
  }

  private onPicked(mode: "light" | "dark", picker: ComboBox): void {
    const option = this.pickerOptions.get(picker)?.[picker.selectedIndex];
    const isUserChoice = !this.isUpdating && option !== undefined;
    if (!isUserChoice) {
      return;
    }
    this.runShowingError({
      kind: "assignTheme",
      mode,
      id: option.id
    });
  }

  private addStore(): void {
    const link = this.storeLink.text.trim();
    if (link.length === 0) {
      return;
    }
    void this.runBusy({
      kind: "addStoreTheme",
      url: link
    }).then(isAdded => {
      if (isAdded) {
        this.storeLink.text = "";
      }
    });
  }

  private async addLocalTheme(kind: LocalThemeKind): Promise<void> {
    const path = await this.pick(kind);
    if (path !== null) {
      await this.runBusy({
        kind: "addLocalTheme",
        path
      });
    }
  }

  // None of runBusy's busy state: holding the add buttons for the save
  // flickered File… and Folder… on every drop.
  private async reorder(ids: string[]): Promise<void> {
    const error = await this.run({
      kind: "reorderThemes",
      ids
    });
    if (error !== null) {
      this.showError(error);
    }
  }

  private runShowingError(action: Action): void {
    void this.runBusy(action);
  }

  private async runBusy(action: Action): Promise<boolean> {
    this.showError("");
    this.setBusy(true);
    const error = await this.run(action);
    this.setBusy(false);
    this.showError(error ?? "");
    return error === null;
  }

  private setBusy(isBusy: boolean): void {
    this.add.isEnabled = !isBusy;
    this.pickFile.isEnabled = !isBusy;
    this.pickFolder.isEnabled = !isBusy;
  }

  private showError(error: string): void {
    this.error.text = error;
    this.error.visibility = error.length > 0 ? Visibility.Visible : Visibility.Collapsed;
  }
}
