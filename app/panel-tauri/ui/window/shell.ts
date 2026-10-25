import type { Bounds, View } from "../protocol.g.ts";
import { autoLength, starLength, thickness } from "./layout.ts";
import * as motion from "./motion.ts";
import { scaleOf } from "./native.ts";
import type { PanelHost } from "./panel-view.ts";
import { PanelView } from "./panel-view.ts";
import { style, StyleKey } from "./styles.ts";
import type { LocalThemeKind, RunAction } from "./theming-card.ts";
import { LocalThemeKind as ThemeKind } from "./theming-card.ts";
import {
  BitmapImage,
  ColumnDefinition,
  DesktopAcrylicBackdrop,
  DisplayArea,
  DisplayAreaFallback,
  FileOpenPicker,
  FolderPicker,
  Grid,
  Image,
  OverlappedPresenter,
  projectAs,
  RowDefinition,
  TextBlock,
  Uri,
  VerticalAlignment,
  Window,
  WindowActivationState
} from "./winui.ts";
import { pathToFileURL } from "node:url";

// In device-independent pixels; only the height is the user's.
const windowWidth = 560;
const windowHeight = 640;
const minimumHeight = 420;
const titleBarHeight = 40;
const titleBarInset = 14;
const iconSize = 16;
const titleGap = 10;
const statusGap = 12;
const statusShown = 1;
const statusHidden = 0;
const statusMilliseconds = 4000;
const themeFileTypes = [".crx", ".zip"];

// What the window needs from the process it runs in: the core's link.
export interface ShellLink {
  run: RunAction;
  refresh(): void;
  restartToUpdate(): Promise<string | null>;
  reportBounds(bounds: Bounds): void;
  setTimeout(milliseconds: number, callback: () => void): () => void;
}

// The window: dressed like any-stt's panel (acrylic, the content running into
// the title bar), so the two tools read as one family.
export class Shell {
  private readonly window = new Window();
  private readonly status = new TextBlock();
  private readonly panel: PanelView;
  private readonly statusTimer: ShellLink["setTimeout"];
  private cancelStatus: (() => void) | null = null;

  constructor(view: View, link: ShellLink, placement: Bounds | null) {
    this.window.systemBackdrop = new DesktopAcrylicBackdrop();
    this.window.extendsContentIntoTitleBar = true;
    this.window.title = view.productName;
    this.window.appWindow.setIcon(view.icon);

    const titleBar = this.titleBar(view.productName, view.icon);
    const layout = new Grid();
    for (const height of [autoLength(), starLength()]) {
      const row = new RowDefinition();
      row.height = height;
      layout.rowDefinitions.append(row);
    }
    layout.children.append(titleBar);
    const host: PanelHost = {
      window: this.window,
      run: link.run,
      pick: kind => this.pick(kind),
      refresh: link.refresh,
      restartToUpdate: link.restartToUpdate
    };
    this.panel = new PanelView(host, view);
    Grid.setRow(this.panel.root, 1);
    layout.children.append(this.panel.root);
    this.window.content = layout;
    this.window.setTitleBar(titleBar);

    this.fixWidth(placement);
    this.window.onActivated((_sender, event) => {
      if (event.windowActivationState !== WindowActivationState.Deactivated) {
        motion.refresh();
        link.refresh();
      }
    });
    this.window.appWindow.onChanged(() => {
      const { position, size } = this.window.appWindow;
      link.reportBounds({
        x: position.x,
        y: position.y,
        width: size.width,
        height: size.height
      });
    });
    this.statusTimer = link.setTimeout;
  }

  onClosed(callback: () => void): void {
    this.window.onClosed(callback);
  }

  activate(): void {
    this.window.activate();
  }

  render(view: View): void {
    this.panel.render(view);
  }

  showActionNotice(text: string): void {
    this.panel.showActionNotice(text);
  }

  // Launching Edge+ again lands here.
  bringBack(): void {
    projectAs(this.window.appWindow.presenter, OverlappedPresenter).restore();
    this.window.activate();
  }

  close(): void {
    this.window.close();
  }

  showStatus(line: string): void {
    this.status.text = line;
    this.status.opacity = statusShown;
    this.cancelStatus?.();
    this.cancelStatus = this.statusTimer(statusMilliseconds, () => {
      this.status.opacity = statusHidden;
    });
  }

  private titleBar(productName: string, iconPath: string): Grid {
    const bar = new Grid();
    bar.height = titleBarHeight;
    bar.padding = thickness(titleBarInset, 0, 0, 0);
    for (const width of [autoLength(), autoLength(), starLength()]) {
      const column = new ColumnDefinition();
      column.width = width;
      bar.columnDefinitions.append(column);
    }
    const icon = new Image();
    icon.width = iconSize;
    icon.height = iconSize;
    icon.verticalAlignment = VerticalAlignment.Center;
    icon.source = new BitmapImage(new Uri(pathToFileURL(iconPath).href));
    bar.children.append(icon);
    const name = new TextBlock();
    name.text = productName;
    name.verticalAlignment = VerticalAlignment.Center;
    name.margin = thickness(titleGap, 0, 0, 0);
    name.style = style(StyleKey.Caption);
    Grid.setColumn(name, 1);
    bar.children.append(name);
    this.status.verticalAlignment = VerticalAlignment.Center;
    this.status.margin = thickness(statusGap, 0, 0, 0);
    this.status.opacity = statusHidden;
    this.status.style = style(StyleKey.CardDescription);
    Grid.setColumn(this.status, 2);
    bar.children.append(this.status);
    return bar;
  }

  // A dev swap places the new window where the old one was; otherwise it is
  // centred on its screen.
  private fixWidth(placement: Bounds | null): void {
    const appWindow = this.window.appWindow;
    const scale = scaleOf(appWindow.id.value);
    const width = Math.round(windowWidth * scale);
    const presenter = projectAs(appWindow.presenter, OverlappedPresenter);
    presenter.isMaximizable = false;
    presenter.preferredMinimumWidth = width;
    presenter.preferredMaximumWidth = width;
    presenter.preferredMinimumHeight = Math.round(minimumHeight * scale);

    if (placement !== null) {
      appWindow.moveAndResize({
        x: placement.x,
        y: placement.y,
        width,
        height: placement.height
      });
      return;
    }

    appWindow.resize({
      width,
      height: Math.round(windowHeight * scale)
    });
    const display = DisplayArea.getFromWindowId(appWindow.id, DisplayAreaFallback.Nearest);
    const area = display.workArea;
    appWindow.move({
      x: area.x + Math.round((area.width - appWindow.size.width) / 2),
      y: area.y + Math.round((area.height - appWindow.size.height) / 2)
    });
  }

  private pick(kind: LocalThemeKind): Promise<string | null> {
    return kind === ThemeKind.Folder ? this.pickFolder() : this.pickFile();
  }

  private async pickFolder(): Promise<string | null> {
    const folder = await new FolderPicker(this.window.appWindow.id).pickSingleFolderAsync();
    return folder?.path ?? null;
  }

  private async pickFile(): Promise<string | null> {
    const picker = new FileOpenPicker(this.window.appWindow.id);
    for (const fileType of themeFileTypes) {
      picker.fileTypeFilter?.append(fileType);
    }

    const file = await picker.pickSingleFileAsync();
    return file?.path ?? null;
  }
}
