import type {
  Action,
  CardView,
  InjectView,
  SwitchKey,
  View
} from "../protocol.g.ts";
import { FeatureCard } from "./feature-card.ts";
import { InjectCard } from "./inject-card.ts";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import PanelPage from "./panel-page.svelte";
import type { PickLocalTheme, RunAction } from "./theming-card.ts";
import { ThemingCard } from "./theming-card.ts";
import { UpdateCard } from "./update-card.ts";
import type { FrameworkElement, Window } from "./winui.ts";
import { DataPackageOperation, DragEventArgs, projectAs, StandardDataFormats } from "./winui.ts";

async function droppedPaths(event: DragEventArgs): Promise<string[]> {
  const items = await event.dataView.getStorageItemsAsync();
  const paths: string[] = [];
  for (const item of items ?? []) {
    paths.push(item.path);
  }

  return paths;
}

// What the page asks of the window it is in.
export interface PanelHost {
  readonly window: Window;
  run: RunAction;
  pick: PickLocalTheme;
  refresh(): void;
  restartToUpdate(): Promise<string | null>;
}

// The window's content (panel-page.svelte). It renders the View the core sent
// last and never touches settings, Edge or the DLL itself: every change is an
// Action the core runs, answered with a fresh View. The cards are made here,
// once per key, since each holds its own control's state.
export class PanelView {
  readonly root: FrameworkElement;
  private readonly page: Mounted<Record<string, unknown>>;
  private readonly switchCards = new Map<SwitchKey, FeatureCard>();
  private readonly cardControls = new Map<SwitchKey, FrameworkElement>();
  private themingCard: ThemingCard | null = null;
  private readonly updateCard: UpdateCard;
  private injectCard: InjectCard | null = null;
  private readonly host: PanelHost;

  constructor(host: PanelHost, view: View) {
    this.host = host;
    this.updateCard = new UpdateCard(() => {
      void this.restartToUpdate();
    });
    this.makeCards(view);
    this.page = mountControl(PanelPage, {
      sections: view.sections,
      cardControl: (key: SwitchKey) => this.cardControl(key),
      updateCard: this.updateCard.view,
      injectCard: null,
      openEdgeHint: view.openEdgeHint,
      buildNotice: view.buildNotice,
      actionNotice: "",
      footer: view.footer,
      dropHint: view.dropHint,
      isVeilShown: false,
      onDragOver: (_sender: unknown, event: unknown) => {
        this.onDragOver(projectAs(event, DragEventArgs));
      },
      onDragLeave: () => {
        this.page.set({ isVeilShown: false });
      },
      onDrop: (_sender: unknown, event: unknown) => {
        void this.onDrop(projectAs(event, DragEventArgs));
      },
      onActionNoticeClosed: () => {
        this.showActionNotice("");
      }
    });
    this.root = this.page.root;
    this.render(view);
  }

  render(view: View): void {
    this.makeCards(view);
    this.page.set({
      sections: view.sections,
      openEdgeHint: view.openEdgeHint,
      buildNotice: view.buildNotice
    });
    for (const card of view.sections.flatMap(section => section.cards)) {
      this.renderCard(card);
    }

    this.updateCard.show(view.update);
  }

  showActionNotice(message: string): void {
    this.page.set({ actionNotice: message });
  }

  private renderCard(card: CardView): void {
    const theming = card.theming;
    if (theming !== null) {
      this.themingCard?.update(card, theming);
      return;
    }

    this.switchCards.get(card.key)?.update(card.isOn, card.isLocked, card.note);

    if (card.inject !== null) {
      this.showInjectOffer(card.inject.offer);
    }
  }

  // Made before the page places them: a card mounts a component of its own,
  // which cannot happen while the page is rendering.
  private makeCards(view: View): void {
    for (const card of view.sections.flatMap(section => section.cards)) {
      if (!this.cardControls.has(card.key)) {
        this.cardControls.set(card.key, this.cardView(card));
      }
    }
  }

  private cardControl(key: SwitchKey): FrameworkElement {
    const control = this.cardControls.get(key);
    if (control === undefined) {
      throw new Error(`no ${key} card was made`);
    }

    return control;
  }

  private cardView(card: CardView): FrameworkElement {
    if (card.theming !== null) {
      const theming = new ThemingCard(card, card.theming, this.host.run, this.host.pick);
      this.themingCard = theming;
      return theming.view;
    }

    const feature = new FeatureCard(card.wording, isOn => {
      void this.runAndReport({
        kind: "setSwitch",
        key: card.key,
        isOn
      });
    });
    this.switchCards.set(card.key, feature);
    return feature.view;
  }

  private showInjectOffer(offer: InjectView | null): void {
    if (offer === null) {
      this.injectCard = null;
      this.page.set({ injectCard: null });
      return;
    }

    if (this.injectCard === null) {
      const card = new InjectCard(offer, () => {
        void this.inject(card);
      });
      this.injectCard = card;
      this.page.set({ injectCard: card.view });
      return;
    }

    this.injectCard.update(offer);
  }

  private async inject(card: InjectCard): Promise<void> {
    await this.runAndReport({ kind: "inject" });
    // A failed injection renders no new view; the button the click disabled
    // comes back as the offer last shown.
    card.restore();
  }

  private async restartToUpdate(): Promise<void> {
    const error = await this.host.restartToUpdate();
    if (error !== null) {
      this.updateCard.reset();
      this.showActionNotice(error);
    }
  }

  private async runAndReport(action: Action): Promise<void> {
    const error = await this.host.run(action);
    if (error !== null) {
      this.showActionNotice(error);
    }
  }

  private onDragOver(event: DragEventArgs): void {
    const hasFiles = event.dataView.contains(StandardDataFormats.storageItems);
    if (!hasFiles) {
      return;
    }

    event.acceptedOperation = DataPackageOperation.Copy;
    this.page.set({ isVeilShown: true });
  }

  // The core tells a folder, .zip and .crx apart; the last failure is the one
  // shown.
  private async onDrop(event: DragEventArgs): Promise<void> {
    this.page.set({ isVeilShown: false });
    const hasFiles = event.dataView.contains(StandardDataFormats.storageItems);
    if (!hasFiles) {
      return;
    }

    this.showActionNotice("");
    for (const path of await droppedPaths(event)) {
      await this.addDropped(path);
    }
  }

  private async addDropped(path: string): Promise<void> {
    const error = await this.host.run({
      kind: "addLocalTheme",
      path
    });
    if (error !== null) {
      this.showActionNotice(error);
    }
  }
}
