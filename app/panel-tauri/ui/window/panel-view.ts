import type { Action, CardView, InjectView, SectionView, View } from "../protocol.ts";
import type { PickLocalTheme, RunAction } from "./theming-card.ts";
import type { FrameworkElement, Window } from "./winui.ts";
import { FeatureCard } from "./feature-card.ts";
import { InjectCard } from "./inject-card.ts";
import { autoLength, boxed, milliseconds, starLength, thickness, uniform } from "./layout.ts";
import * as motion from "./motion.ts";
import { style, StyleKey } from "./styles.ts";
import { ThemingCard } from "./theming-card.ts";
import { UpdateCard } from "./update-card.ts";
import {
  ColumnDefinition,
  DataPackageOperation,
  DoubleCollection,
  DragEventArgs,
  Grid,
  HorizontalAlignment,
  HyperlinkButton,
  InfoBar,
  InfoBarSeverity,
  Orientation,
  projectAs,
  Rectangle,
  ScalarTransition,
  ScrollBarVisibility,
  ScrollViewer,
  StackPanel,
  StandardDataFormats,
  TextBlock,
  Uri,
  VerticalAlignment,
  Visibility
} from "./winui.ts";

const veilShown = 1;
const veilHidden = 0;
const contentPadding = 16;
const cardSpacing = 4;
const sectionSpacing = 20;
const veilInset = 12;
const veilCornerRadius = 8;
const veilDashes = [4, 3];
const veilFadeMilliseconds = 120;
// Lines the header up with the cards' text, as Windows Settings does.
const sectionHeaderMargin = thickness(4, 0, 0, 4);

// What the page asks of the window it is in.
export interface PanelHost {
  readonly window: Window;
  run: RunAction;
  pick: PickLocalTheme;
  refresh(): void;
  restartToUpdate(): Promise<string | null>;
}

// The window's content. It renders the View the core sent last and never
// touches settings, Edge or the DLL itself: every change is an Action the core
// runs, answered with a fresh View.
export class PanelView {
  readonly root = new Grid();
  private readonly sectionList = new StackPanel();
  private readonly openEdgeHint = new TextBlock();
  private readonly buildNotice = new InfoBar();
  private readonly actionNotice = new InfoBar();
  private readonly dropVeil: Grid;
  private readonly switchCards = new Map<string, FeatureCard>();
  private readonly themingCards: ThemingCard[] = [];
  private readonly updateCard: UpdateCard;
  // The Inject automatically card, with the Inject card under it while offered;
  // its add/delete transition slides the Inject card in and out.
  private readonly injectGroup = new StackPanel();
  private injectCard: InjectCard | null = null;
  private readonly host: PanelHost;

  constructor(host: PanelHost, view: View) {
    this.host = host;
    this.root.allowDrop = true;
    this.openEdgeHint.style = style(StyleKey.CardDescription);
    this.openEdgeHint.margin = thickness(4, 0, 4, 4);
    this.openEdgeHint.visibility = Visibility.Collapsed;
    this.buildNotice.isClosable = false;
    this.buildNotice.severity = InfoBarSeverity.Warning;
    this.actionNotice.isClosable = true;
    this.actionNotice.severity = InfoBarSeverity.Error;
    this.updateCard = new UpdateCard(() => void this.restartToUpdate());
    this.injectGroup.spacing = cardSpacing;
    motion.animate(this.injectGroup, motion.addDelete);
    this.sectionList.spacing = sectionSpacing;
    for (const section of view.sections) {
      this.sectionList.children.append(this.sectionView(section));
    }
    motion.animate(this.sectionList, motion.reposition);

    const page = new StackPanel();
    page.padding = thickness(contentPadding, cardSpacing, contentPadding, contentPadding);
    page.spacing = cardSpacing;
    page.children.append(this.updateCard.view);
    page.children.append(this.openEdgeHint);
    page.children.append(this.buildNotice);
    page.children.append(this.actionNotice);
    page.children.append(this.sectionList);
    page.children.append(this.footer(view));
    const scroller = new ScrollViewer();
    scroller.content = page;
    scroller.verticalScrollBarVisibility = ScrollBarVisibility.Auto;
    this.root.children.append(scroller);
    this.dropVeil = this.veil(view.dropHint);
    this.root.children.append(this.dropVeil);
    this.root.onDragOver((_sender, event) => this.onDragOver(projectAs(event, DragEventArgs)));
    this.root.onDragLeave(() => {
      this.dropVeil.opacity = veilHidden;
    });
    this.root.onDrop((_sender, event) => void this.onDrop(projectAs(event, DragEventArgs)));
    this.render(view);
  }

  render(view: View): void {
    const hint = view.openEdgeHint;
    this.openEdgeHint.text = hint ?? "";
    this.openEdgeHint.visibility = hint === null ? Visibility.Collapsed : Visibility.Visible;
    this.buildNotice.message = view.buildNotice;
    this.buildNotice.isOpen = view.buildNotice.length > 0;
    for (const section of view.sections) {
      for (const card of section.cards) {
        this.renderCard(card);
      }
    }
    this.updateCard.show(view.update);
  }

  showActionNotice(message: string): void {
    this.actionNotice.message = message;
    this.actionNotice.isOpen = message.length > 0;
  }

  private renderCard(card: CardView): void {
    if (card.theming !== null) {
      for (const theming of this.themingCards) {
        theming.update(card);
      }
      return;
    }
    this.switchCards.get(card.key)?.update(card.isOn, card.isLocked, card.note);
    if (card.inject !== null) {
      this.showInjectOffer(card.inject.offer);
    }
  }

  private sectionView(section: SectionView): StackPanel {
    const stack = new StackPanel();
    stack.spacing = cardSpacing;
    const header = new TextBlock();
    header.text = section.name;
    header.style = style(StyleKey.SectionHeader);
    header.margin = sectionHeaderMargin;
    stack.children.append(header);
    for (const card of section.cards) {
      stack.children.append(this.cardView(card));
    }
    motion.animate(stack, motion.reposition);
    return stack;
  }

  private cardView(card: CardView): FrameworkElement {
    if (card.theming !== null) {
      const theming = new ThemingCard(card, card.theming, this.host.run, this.host.pick);
      this.themingCards.push(theming);
      return theming.view;
    }
    const feature = new FeatureCard(card.wording, isOn => void this.runAndReport({
      kind: "setSwitch",
      key: card.key,
      isOn
    }));
    this.switchCards.set(card.key, feature);
    if (card.inject === null) {
      return feature.view;
    }
    this.injectGroup.children.append(feature.view);
    return this.injectGroup;
  }

  private showInjectOffer(offer: InjectView | null): void {
    if (offer === null) {
      if (this.injectCard !== null) {
        this.injectGroup.children.removeAtEnd();
        this.injectCard = null;
      }
      return;
    }
    if (this.injectCard === null) {
      const card = new InjectCard(offer, () => void this.inject(card));
      this.injectCard = card;
      this.injectGroup.children.append(card.view);
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

  private footer(view: View): Grid {
    const footer = new Grid();
    footer.margin = thickness(4, 14, 4, 0);
    for (const width of [starLength(), autoLength()]) {
      const column = new ColumnDefinition();
      column.width = width;
      footer.columnDefinitions.append(column);
    }
    const byline = new StackPanel();
    byline.orientation = Orientation.Horizontal;
    byline.spacing = cardSpacing;
    const by = new TextBlock();
    by.text = view.footer.by;
    by.style = style(StyleKey.CardDescription);
    by.verticalAlignment = VerticalAlignment.Center;
    byline.children.append(by);
    const author = new HyperlinkButton();
    author.content = boxed(view.footer.author);
    author.navigateUri = new Uri(view.footer.authorUri);
    author.padding = thickness(2, 0, 2, 0);
    byline.children.append(author);
    const version = new TextBlock();
    version.text = view.footer.version;
    version.style = style(StyleKey.CardDescription);
    version.verticalAlignment = VerticalAlignment.Center;
    Grid.setColumn(version, 1);
    footer.children.append(byline);
    footer.children.append(version);
    return footer;
  }

  private veil(hint: string): Grid {
    const veil = new Grid();
    veil.style = style(StyleKey.DropVeil);
    veil.margin = uniform(veilInset);
    veil.opacity = veilHidden;
    veil.isHitTestVisible = false;
    const fade = new ScalarTransition();
    fade.duration = milliseconds(veilFadeMilliseconds);
    veil.opacityTransition = fade;
    const frame = new Rectangle();
    frame.style = style(StyleKey.DropVeilFrame);
    const dashes = new DoubleCollection();
    for (const dash of veilDashes) {
      dashes.append(dash);
    }
    frame.strokeDashArray = dashes;
    frame.radiusX = veilCornerRadius;
    frame.radiusY = veilCornerRadius;
    veil.children.append(frame);
    const text = new TextBlock();
    text.text = hint;
    text.style = style(StyleKey.CardTitle);
    text.horizontalAlignment = HorizontalAlignment.Center;
    text.verticalAlignment = VerticalAlignment.Center;
    veil.children.append(text);
    return veil;
  }

  private onDragOver(event: DragEventArgs): void {
    const hasFiles = event.dataView.contains(StandardDataFormats.storageItems);
    if (!hasFiles) {
      return;
    }
    event.acceptedOperation = DataPackageOperation.Copy;
    this.dropVeil.opacity = veilShown;
  }

  // The core tells a folder, .zip and .crx apart; the last failure is the one
  // shown.
  private async onDrop(event: DragEventArgs): Promise<void> {
    this.dropVeil.opacity = veilHidden;
    const hasFiles = event.dataView.contains(StandardDataFormats.storageItems);
    if (!hasFiles) {
      return;
    }
    this.showActionNotice("");
    const items = await event.dataView.getStorageItemsAsync();
    if (items === null) {
      return;
    }
    for (let i = 0; i < items.size; i++) {
      const error = await this.host.run({
        kind: "addLocalTheme",
        path: items.getAt(i).path
      });
      if (error !== null) {
        this.showActionNotice(error);
      }
    }
  }
}
