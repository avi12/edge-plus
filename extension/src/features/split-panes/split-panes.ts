// Split into any grid: a tab becomes a grid of panes inside itself, each pane a frame on your own
// profile. Edge's own split view stops at two panes (see docs/edge-split-view.md); this one
// splits any pane again, side by side or stacked, and every tab keeps its own layout.
//
// The split page renders a tab's state from the tab's address (split-state.ts); everything here
// edits that address.

import { allowFramesInSplitTabs, stopAllowingFrames } from "./frame-rule";
import type { Pane } from "./layout";
import * as Layout from "./layout";
import type { PresetId } from "./presets";
import { PRESETS } from "./presets";
import type { SplitState } from "./split-state";
import { isSplitPageUrl, newPane, splitPageUrl, stateOfTab } from "./split-state";
import type { Direction } from "@/features/pane-focus/neighbour";
import { neighbour } from "@/features/pane-focus/neighbour";
import { isFrameable, NEW_TAB_URL } from "@/lib/address";
import type { FeatureModule } from "@/lib/feature";
import { Command } from "@/lib/feature";
import type { Browser } from "wxt/browser";

enum MenuId {
  right = "splitPanes.right",
  down = "splitPanes.down",
  linkRight = "splitPanes.linkRight",
  linkDown = "splitPanes.linkDown"
}

let isEnabled = false;
let report = () => {};

const frameable = (url = "") => isFrameable(url) ? url : "";

async function activeTab(windowId?: number) {
  const [tab] = await browser.tabs.query({ active: true, ...(windowId ? { windowId } : { lastFocusedWindow: true }) });
  const isSplittable = tab?.id !== undefined && !tab.incognito;
  return isSplittable ? tab : null;
}

/** The tab you are on with its split state, or null when it is not a split */
async function activeSplit(windowId?: number) {
  const tab = await activeTab(windowId);
  const state = stateOfTab(tab?.url);
  const isSplitTab = !!tab && !!state;
  if (!isSplitTab) {
    return null;
  }
  return { tab, state };
}

/** The tab's own shape, so new panes split across their longer side on screen */
function aspectOf(tab: Browser.tabs.Tab) {
  const { width, height } = tab;
  const isSizeKnown = !!width && !!height;
  if (!isSizeKnown) {
    return Layout.DEFAULT_ASPECT;
  }
  return width / height;
}

async function writeState({ tab, state }: { tab: Browser.tabs.Tab; state: SplitState }) {
  await allowFramesInSplitTabs(tab.id);
  await browser.tabs.update(tab.id!, { url: splitPageUrl(state) });
}

/** A normal tab becomes the first pane of its own split; a split tab grows beside its focused pane */
async function addPane({ windowId, axis, url = "" }: { windowId?: number; axis: Layout.Axis; url?: string }) {
  const tab = await activeTab(windowId);
  if (!tab) {
    return;
  }
  const pane = newPane(frameable(url));
  const current = stateOfTab(tab.url);
  const first = current ? null : newPane(frameable(tab.url));
  const root = current?.root ?? first!;
  const targetId = current?.activePaneId ?? first!.id;
  await writeState({
    tab,
    state: { root: Layout.split({ root, targetId, newPane: pane, axis }), activePaneId: pane.id }
  });
}

/**
 * Every tab selected in the tab strip (Ctrl + click) moves into panes of one tab, the way Arc
 * makes a split from a selection. A selected split tab takes the rest in; otherwise the tab you
 * are on does. A selected split brings all its panes; Edge's own pages cannot be framed, so they
 * stay tabs
 */
export async function addSelectedTabs(windowId?: number) {
  const tab = await activeTab(windowId);
  if (!tab) {
    return;
  }
  const selected = await selectedTabs(tab.windowId);
  const host = selected.find(other => isSplitPageUrl(other.url)) ?? tab;
  const guests = selected.filter(other => other.id !== host.id);
  const hasGuests = guests.length > 0;
  if (!hasGuests) {
    return;
  }
  const current = stateOfTab(host.url);
  const first = current ? null : newPane(frameable(host.url));
  const added = guests.flatMap(guest => {
    const guestState = stateOfTab(guest.url);
    return guestState ? Layout.panes(guestState.root) : [newPane(guest.url)];
  });
  const root = Layout.grow({ root: current?.root ?? first!, newPanes: added, aspect: aspectOf(host) });
  await writeState({ tab: host, state: { root, activePaneId: added[0]!.id } });
  await browser.tabs.remove(guests.map(guest => guest.id!));
  await browser.tabs.update(host.id!, { active: true });
}

/** Rebuilds the split as an even grid of its panes in reading order; focus stays where it was */
export async function tile(windowId?: number) {
  const split = await activeSplit(windowId);
  if (!split) {
    return;
  }
  const { tab, state } = split;
  const root = Layout.tile({ panes: Layout.panes(state.root), aspect: aspectOf(tab) });
  await writeState({ tab, state: { ...state, root } });
}

/** The selected tabs a pane can show: split tabs and framable pages, never another window's */
async function selectedTabs(windowId: number) {
  const highlighted = await browser.tabs.query({ windowId, highlighted: true });
  return highlighted.filter(other => !other.incognito && (isSplitPageUrl(other.url) || isFrameable(other.url)));
}

/**
 * Lays the tab out as a preset. A normal tab takes the tabs to its right into the other panes, in
 * order; a split tab keeps its panes, and any the preset has no room for go back to being tabs
 */
export async function applyPreset({ windowId, presetId }: { windowId: number; presetId: PresetId }) {
  const preset = PRESETS.find(candidate => candidate.id === presetId);
  const tab = await activeTab(windowId);
  const canApply = !!preset && !!tab;
  if (!canApply) {
    return;
  }
  const slots = Layout.slotCount(preset.template);
  const current = stateOfTab(tab.url);
  const followingTabs = current
    ? []
    : (await browser.tabs.query({ windowId: tab.windowId }))
      .filter(other => other.index > tab.index && !other.pinned && isFrameable(other.url))
      .slice(0, slots - 1);
  const candidates: Pane[] = current
    ? activeFirst(current)
    : [newPane(frameable(tab.url)), ...followingTabs.map(other => newPane(other.url))];
  const chosen = candidates.slice(0, slots);
  while (chosen.length < slots) {
    chosen.push(newPane());
  }
  await writeState({
    tab,
    state: { root: Layout.fromTemplate({ template: preset.template, panes: chosen }), activePaneId: chosen[0]!.id }
  });
  await reopenAsTabs({ tab, urls: candidates.slice(slots).map(pane => pane.url) });
  await browser.tabs.remove(followingTabs.map(other => other.id!));
}

/** The focused pane stays in the tab; every other pane goes back to being a tab right after it */
export async function merge(windowId?: number) {
  const split = await activeSplit(windowId);
  if (!split) {
    return;
  }
  const { tab, state } = split;
  const [kept, ...others] = activeFirst(state);
  await browser.tabs.update(tab.id!, { url: kept!.url || NEW_TAB_URL });
  await reopenAsTabs({ tab, urls: others.map(pane => pane.url) });
}

export async function describe(windowId: number) {
  const tab = await activeTab(windowId);
  const state = stateOfTab(tab?.url);
  return {
    panes: state ? Layout.panes(state.root).length : 1,
    selectedTabs: tab ? (await selectedTabs(tab.windowId)).length : 0
  };
}

/** Moves the focused pane to its neighbour on that side, the way a tiling window manager does */
export async function focusNeighbour(direction: Direction) {
  const split = await activeSplit();
  if (!split) {
    return;
  }
  const { tab, state } = split;
  const target = neighbour({ rects: Layout.layout({ root: state.root }), fromPaneId: state.activePaneId, direction });
  if (target) {
    await writeState({ tab, state: { ...state, activePaneId: target } });
  }
}

export const splitRight = (windowId?: number) => addPane({ windowId, axis: Layout.Axis.row });

export const splitDown = (windowId?: number) => addPane({ windowId, axis: Layout.Axis.column });

function activeFirst(state: SplitState) {
  const all = Layout.panes(state.root);
  const active = all.find(pane => pane.id === state.activePaneId) ?? all[0]!;
  return [active, ...all.filter(pane => pane !== active)];
}

async function reopenAsTabs({ tab, urls }: { tab: Browser.tabs.Tab; urls: string[] }) {
  for (const [i, url] of urls.filter(Boolean).entries()) {
    await browser.tabs.create({ windowId: tab.windowId, index: tab.index + 1 + i, url, active: false });
  }
}

async function countSplits() {
  let splits = 0;
  let panes = 0;
  for (const tab of await browser.tabs.query({})) {
    const state = stateOfTab(tab.url);
    if (state) {
      splits++;
      panes += Layout.panes(state.root).length;
    }
  }
  return { splits, panes };
}

let lastStatus = { splits: 0, panes: 0 };

async function refresh() {
  if (!isEnabled) {
    return;
  }
  await allowFramesInSplitTabs();
  lastStatus = await countSplits();
  report();
}

/** Added once, when the background starts, so Edge can wake the worker for them */
export function listen() {
  browser.runtime.onStartup.addListener(refresh);
  browser.tabs.onRemoved.addListener(refresh);
  browser.tabs.onUpdated.addListener((_, { url }) => {
    const isSplitChange = url !== undefined;
    if (isSplitChange) {
      void refresh();
    }
  });
}

export const splitPanes: FeatureModule = {
  async enable(context) {
    report = context.report;
    isEnabled = true;
    await browser.contextMenus.removeAll();
    browser.contextMenus.create({ id: MenuId.right, title: "Split right", contexts: ["page", "frame"] });
    browser.contextMenus.create({ id: MenuId.down, title: "Split down", contexts: ["page", "frame"] });
    browser.contextMenus.create({ id: MenuId.linkRight, title: "Open link in a pane to the right", contexts: ["link"] });
    browser.contextMenus.create({ id: MenuId.linkDown, title: "Open link in a pane below", contexts: ["link"] });
    await refresh();
  },
  // Every split folds back into tabs, so switching the feature off loses no page
  async disable() {
    isEnabled = false;
    for (const tab of await browser.tabs.query({})) {
      if (isSplitPageUrl(tab.url)) {
        await merge(tab.windowId);
      }
    }
    await stopAllowingFrames();
    await Promise.all(Object.values(MenuId).map(id => browser.contextMenus.remove(id).catch(() => {})));
  },
  status: () => lastStatus,
  commands: {
    [Command.splitRight]: splitRight,
    [Command.splitDown]: splitDown,
    [Command.unsplit]: merge,
    [Command.addSelectedTabs]: addSelectedTabs,
    [Command.tile]: tile
  },
  async onMenu(info, tab) {
    const windowId = tab?.windowId;
    const url = info.linkUrl;
    switch (info.menuItemId) {
      case MenuId.right:
      case MenuId.linkRight:
        return addPane({ windowId, axis: Layout.Axis.row, url });
      case MenuId.down:
      case MenuId.linkDown:
        return addPane({ windowId, axis: Layout.Axis.column, url });
    }
  }
};

