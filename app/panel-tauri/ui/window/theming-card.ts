import type { Action, CardView, ThemingView } from "../protocol.g.ts";
import { pickerRow, twoColumnRow } from "./card-rows.ts";
import { EdgeThemeList } from "./edge-theme-list.ts";
import { FeatureCard } from "./feature-card.ts";
import { boxed } from "./layout.ts";
import * as motion from "./motion.ts";
import { OwnThemeList } from "./own-theme-list.ts";
import { style, StyleKey } from "./styles.ts";
import { ThemePicker } from "./theme-picker.ts";
import type { FrameworkElement, Grid } from "./winui.ts";
import {
  Button,
  KeyRoutedEventArgs,
  Orientation,
  projectAs,
  StackPanel,
  TextBlock,
  TextBox,
  VerticalAlignment,
  VirtualKey,
  Visibility
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
const fullOpacity = 1;

// The theming switch, with the theme library under it: add from a store link
// or a local file, pick a theme for each Windows mode, and order the list.
export class ThemingCard {
  private readonly card: FeatureCard;
  private readonly body = new StackPanel();
  private readonly storeLink = new TextBox();
  private readonly add = new Button();
  private readonly pickFile = new Button();
  private readonly pickFolder = new Button();
  private readonly error = new TextBlock();
  private readonly lightPicker: ThemePicker;
  private readonly darkPicker: ThemePicker;
  private readonly ownThemes: OwnThemeList;
  private readonly edgeThemes = new EdgeThemeList();
  private readonly run: RunAction;
  private readonly pick: PickLocalTheme;

  // `theming` is the card's first view, which carries its labels.
  constructor(view: CardView, theming: ThemingView, run: RunAction, pick: PickLocalTheme) {
    this.run = run;
    this.pick = pick;
    this.card = new FeatureCard(view.wording, isOn => this.runShowingError({
      kind: "setSwitch",
      key: view.key,
      isOn
    }));
    this.lightPicker = new ThemePicker(id => this.runShowingError({
      kind: "assignTheme",
      mode: "light",
      id
    }));
    this.darkPicker = new ThemePicker(id => this.runShowingError({
      kind: "assignTheme",
      mode: "dark",
      id
    }));
    this.ownThemes = new OwnThemeList(theming.labels, {
      remove: id => this.runShowingError({
        kind: "removeTheme",
        id
      }),
      reorder: ids => this.runQuietly({
        kind: "reorderThemes",
        ids
      }),
      move: (id, offset) => this.runQuietly({
        kind: "moveTheme",
        id,
        offset
      })
    });
    this.error.style = style(StyleKey.CardError);
    this.error.visibility = Visibility.Collapsed;
    this.body.spacing = rowSpacing;
    this.body.children.append(this.storeRow(theming));
    this.body.children.append(this.localRow(theming));
    this.body.children.append(this.error);
    this.body.children.append(pickerRow(theming.labels.whenLight, this.lightPicker.box));
    this.body.children.append(pickerRow(theming.labels.whenDark, this.darkPicker.box));
    this.body.children.append(this.ownThemes.view);
    this.body.children.append(this.edgeThemes.view);
    this.card.setBody(this.body);
    motion.animate(this.body, motion.reposition);
  }

  get view(): FrameworkElement {
    return this.card.view;
  }

  update(view: CardView, theming: ThemingView): void {
    this.card.update(view.isOn, view.isLocked, view.note);
    this.body.opacity = view.isOn ? fullOpacity : dimmedOpacity;
    this.lightPicker.fill(theming.options, theming.light);
    this.darkPicker.fill(theming.options, theming.dark);
    this.ownThemes.update(theming.own);
    this.edgeThemes.update(theming.edge, theming.empty);
  }

  private storeRow(theming: ThemingView): Grid {
    this.storeLink.placeholderText = theming.labels.storeLinkPlaceholder;
    this.storeLink.onKeyDown((...args: unknown[]) => {
      if (projectAs(args[1], KeyRoutedEventArgs).key === VirtualKey.Enter) {
        void this.addStore();
      }
    });
    this.add.content = boxed(theming.labels.add);
    this.add.style = style(StyleKey.AccentButton);
    this.add.onClick(() => {
      void this.addStore();
    });
    return twoColumnRow(this.storeLink, this.add);
  }

  private localRow(theming: ThemingView): StackPanel {
    const label = new TextBlock();
    label.text = theming.labels.addLocal;
    label.style = style(StyleKey.CardDescription);
    label.verticalAlignment = VerticalAlignment.Center;
    this.pickFile.content = boxed(theming.labels.pickFile);
    this.pickFile.onClick(() => {
      void this.addLocal(LocalThemeKind.File);
    });
    this.pickFolder.content = boxed(theming.labels.pickFolder);
    this.pickFolder.onClick(() => {
      void this.addLocal(LocalThemeKind.Folder);
    });
    const row = new StackPanel();
    row.orientation = Orientation.Horizontal;
    row.spacing = columnGap;
    row.children.append(label);
    row.children.append(this.pickFile);
    row.children.append(this.pickFolder);
    return row;
  }

  private async addStore(): Promise<void> {
    const link = this.storeLink.text.trim();
    if (link.length === 0) {
      return;
    }

    const isAdded = await this.runBusy({
      kind: "addStoreTheme",
      url: link
    });
    if (isAdded) {
      this.storeLink.text = "";
    }
  }

  private async addLocal(kind: LocalThemeKind): Promise<void> {
    const path = await this.pick(kind);
    if (path !== null) {
      await this.runBusy({
        kind: "addLocalTheme",
        path
      });
    }
  }

  private runShowingError(action: Action): void {
    void this.runBusy(action);
  }

  // None of runBusy's busy state: holding the add buttons for a reorder
  // flickered File… and Folder… on every drop.
  private async runQuietly(action: Action): Promise<void> {
    const error = await this.run(action);
    if (error !== null) {
      this.showError(error);
    }
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
