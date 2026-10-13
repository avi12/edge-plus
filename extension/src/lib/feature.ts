import type { Browser } from "wxt/browser";

/** The manifest's command names, so they are the contract with the user's own key bindings */
export enum Command {
  splitRight = "split-right",
  splitDown = "split-down",
  unsplit = "unsplit",
  addSelectedTabs = "add-selected-tabs",
  tile = "tile",
  focusLeft = "focus-left",
  focusRight = "focus-right",
  focusUp = "focus-up",
  focusDown = "focus-down"
}

/** What every feature hands the background, which switches it on and off live */
export type FeatureModule = {
  enable: (context: { report: () => void }) => Promise<void>;
  disable: () => Promise<void>;
  status?: () => Record<string, number>;
  commands?: Partial<Record<Command, (windowId?: number) => Promise<void>>>;
  onMenu?: (info: Browser.contextMenus.OnClickData, tab?: Browser.tabs.Tab) => Promise<void>;
};
