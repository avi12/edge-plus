import type { Action, CardView, ThemingView } from "../protocol.g.ts";
import { EdgeThemeList } from "./edge-theme-list.ts";
import { FeatureCard } from "./feature-card.ts";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import { OwnThemeList } from "./own-theme-list.ts";
import { ThemePicker } from "./theme-picker.ts";
import ThemingBody from "./theming-body.svelte";
import type { FrameworkElement } from "./winui.ts";

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

// The theming switch, with the theme library (theming-body.svelte) under it.
// The pickers and lists are made here, and every action runs from here, so its
// error shows under the add rows.
export class ThemingCard {
  private readonly card: FeatureCard;
  private readonly body: Mounted<Record<string, unknown>>;
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
    this.body = mountControl(ThemingBody, {
      labels: theming.labels,
      isOn: view.isOn,
      isBusy: false,
      error: "",
      lightPicker: this.lightPicker.box,
      darkPicker: this.darkPicker.box,
      ownThemes: this.ownThemes.view,
      edgeThemes: this.edgeThemes.view,
      addStoreTheme: (link: string) => this.runBusy({
        kind: "addStoreTheme",
        url: link
      }),
      pickFile: () => {
        void this.addLocal(LocalThemeKind.File);
      },
      pickFolder: () => {
        void this.addLocal(LocalThemeKind.Folder);
      }
    });
    this.card.setBody(this.body.root);
  }

  get view(): FrameworkElement {
    return this.card.view;
  }

  update(view: CardView, theming: ThemingView): void {
    this.card.update(view.isOn, view.isLocked, view.note);
    this.body.set({ isOn: view.isOn });
    this.lightPicker.fill(theming.options, theming.light);
    this.darkPicker.fill(theming.options, theming.dark);
    this.ownThemes.update(theming.own);
    this.edgeThemes.update(theming.edge, theming.empty);
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
    this.body.set({ isBusy: true });
    const error = await this.run(action);
    this.body.set({ isBusy: false });
    this.showError(error ?? "");
    return error === null;
  }

  private showError(error: string): void {
    this.body.set({ error });
  }
}
