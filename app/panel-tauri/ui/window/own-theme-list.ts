import type { ThemeRow, ThemingLabels } from "../protocol.g.ts";
import { syncKeyed } from "./keyed-list.ts";
import * as motion from "./motion.ts";
import { style, StyleKey } from "./styles.ts";
import {
  idOf,
  keyText,
  nameOf,
  rowKey,
  themeRow
} from "./theme-row.ts";
import type { ContainerContentChangingEventArgs } from "./winui.ts";
import {
  AutomationProperties,
  ContentControl,
  DataTemplate,
  DependencyObject,
  FocusState,
  IVector_Object,
  KeyRoutedEventArgs,
  ListView,
  ListViewItem,
  ListViewSelectionMode,
  projectAs,
  PropertyValue,
  VirtualKey,
  Visibility,
  VisualStateManager,
  VisualTreeHelper,
  XamlReader
} from "./winui.ts";
import { DynWinRtType, DynWinRtValue } from "@microsoft/dynwinrt";

const normalState = "Normal";
const moveUp = -1;
const moveDown = 1;
// Parsed at run time: the slot each row is put in.
const rowSlotTemplate =
  "<DataTemplate xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">" +
  "<ContentControl IsTabStop=\"False\" HorizontalContentAlignment=\"Stretch\" " +
  "HorizontalAlignment=\"Stretch\"/></DataTemplate>";

// Up or down the list for Alt+Up / Alt+Down; null for any other key.
function moveOffset(event: KeyRoutedEventArgs): number | null {
  if (!event.keyStatus.isMenuKeyDown) {
    return null;
  }

  const offsets = new Map<number, number>([[VirtualKey.Up, moveUp], [VirtualKey.Down, moveDown]]);
  return offsets.get(event.key) ?? null;
}

export interface OwnThemeActions {
  remove(id: string): void;
  // The list in the order the user dragged it to.
  reorder(ids: string[]): void;
  // Alt+Up / Alt+Down on a row; resolves once the new order is shown.
  move(id: string, offset: number): Promise<void>;
}

// The user's own themes, in the order of the pickers, reordered by dragging
// or with Alt+Up / Alt+Down.
export class OwnThemeList {
  // No selection: with it on, each dragged row kept the accent bar, so two
  // drags looked like two selected themes. Alt+Up / Alt+Down act on the
  // focused row instead.
  readonly view = new ListView();
  // The list's items are the rows' keys, not the rows: the built-in reorder
  // only commits a dragged move to a list of plain items. Each container shown
  // gets a freshly built row (fill), so a row is never moved between recycled
  // containers.
  private readonly keys = IVector_Object.from(DynWinRtValue.createVector([], DynWinRtType.object()));
  private rows = new Map<string, ThemeRow>();
  private readonly actions: OwnThemeActions;

  constructor(labels: ThemingLabels, actions: OwnThemeActions) {
    this.actions = actions;
    this.view.selectionMode = ListViewSelectionMode.None;
    this.view.isItemClickEnabled = false;
    this.view.canReorderItems = true;
    this.view.canDragItems = true;
    this.view.allowDrop = true;
    this.view.itemContainerStyle = style(StyleKey.ThemeListItem);
    this.view.itemTemplate = projectAs(XamlReader.load(rowSlotTemplate), DataTemplate);
    this.view.itemsSource = this.keys;
    AutomationProperties.setName(this.view, labels.themeListName);
    AutomationProperties.setHelpText(this.view, labels.reorderHelp);
    this.view.onContainerContentChanging((_sender, args) => this.fill(args));
    this.view.onDragItemsCompleted(() => {
      // WinUI can leave the dragged (or last pressed) row in its Pressed or
      // PointerOver look after a drop; every row goes back to Normal.
      this.resetRowStates();
      this.actions.reorder(this.order());
    });
    // Preview: the list's items take arrow keys for moving focus before a
    // bubbling KeyDown would see them.
    this.view.onPreviewKeyDown((...args: unknown[]) => {
      this.onKeyDown(projectAs(args[1], KeyRoutedEventArgs));
    });
    motion.animateItems(this.view, motion.listItems);
  }

  update(rows: ThemeRow[]): void {
    this.rows = new Map(rows.map(row => [rowKey(row), row]));
    const keys = this.keys;
    syncKeyed({
      items: {
        get size() {
          return keys.size;
        },
        getAt: index => keyText(keys.getAt(index)),
        removeAt: index => keys.removeAt(index),
        insertAt: (index, key) => keys.insertAt(index, PropertyValue.createString(key))
      },
      keyOf: key => key,
      create: key => key
    }, [...this.rows.keys()]);
    this.resetRowStates();
    this.view.visibility = rows.length > 0 ? Visibility.Visible : Visibility.Collapsed;
  }

  // The row goes in the slot its template made. Setting the container's own
  // Content instead does not hold: the list writes the item over it.
  private fill(args: ContainerContentChangingEventArgs): void {
    const key = keyText(args.item);
    const row = this.rows.get(key);
    const hasRow = !args.inRecycleQueue && row !== undefined;
    if (!hasRow) {
      return;
    }

    const slot = projectAs(args.itemContainer.contentTemplateRoot, ContentControl);
    slot.content = themeRow(row, () => this.actions.remove(row.id));
    AutomationProperties.setName(args.itemContainer, nameOf(key));
  }

  private order(): string[] {
    const order: string[] = [];
    for (let i = 0; i < this.keys.size; i++) {
      order.push(idOf(keyText(this.keys.getAt(i))));
    }

    return order;
  }

  // Alt+Up / Alt+Down: a reorder for whoever does not drag.
  private onKeyDown(event: KeyRoutedEventArgs): void {
    const offset = moveOffset(event);
    if (offset === null) {
      return;
    }

    const index = this.focusedIndex(event.originalSource);
    if (index < 0) {
      return;
    }

    event.handled = true;
    void this.moveKeepingFocus(idOf(keyText(this.keys.getAt(index))), offset);
  }

  private async moveKeepingFocus(id: string, offset: number): Promise<void> {
    await this.actions.move(id, offset);
    const container = this.view.containerFromIndex(this.order().indexOf(id));
    if (container !== null) {
      projectAs(container, ListViewItem).focus(FocusState.Keyboard);
    }
  }

  // Focus is on the list item itself or on its remove button.
  private focusedIndex(focused: unknown): number {
    let element: DependencyObject | null = focused === null ? null : projectAs(focused, DependencyObject);
    while (element !== null) {
      const index = this.view.indexFromContainer(element);
      if (index >= 0) {
        return index;
      }

      element = VisualTreeHelper.getParent(element);
    }

    return -1;
  }

  private resetRowStates(): void {
    for (let i = 0; i < this.keys.size; i++) {
      const container = this.view.containerFromIndex(i);
      if (container !== null) {
        VisualStateManager.goToState(projectAs(container, ListViewItem), normalState, false);
      }
    }
  }
}
