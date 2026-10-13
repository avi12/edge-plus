// Split into any grid, per tab, the way Arc does it: every tab may own a split of its own size and
// shape, and switching tabs switches the panes with it.
//
// Edge's own split view stops at two panes (see docs/edge-split-view.md), so the panes live outside
// it. The window holding the tab strip is the host and always shows the split's first pane; every
// other pane is a companion window tiled beside it while its tab is active and minimized while not.

import type { Node, Rect } from "./layout";
import * as Layout from "./layout";
import type { PresetId } from "./presets";
import { PRESETS } from "./presets";
import type { FeatureModule } from "@/lib/feature";
import { Command } from "@/lib/feature";
import { z } from "@/lib/zod";
import { storage } from "#imports";
import type { Browser } from "wxt/browser";

const GAP = 0;
/** Windows 11's invisible resize border, in the pixels Edge's bounds use */
const INVISIBLE_BORDER = 7;
/** A user's drag lands as a burst of bounds events; act on the last one */
const SETTLE_MS = 150;
/** How long focus changes are ours after raising a split, not the user's */
const RAISE_QUIET_MS = 400;
/** A window id no real window has, for the pane being created */
const PENDING_PANE = -1;
const BOUNDS_TOLERANCE = 2;
/**
 * Edge will not make a tabbed window narrower than 516 or shorter than 150, measured as its bounds;
 * every pane in a split grows by the invisible border on at least one side, which counts toward it
 */
const MINIMUM_PANE = { width: 516 - INVISIBLE_BORDER, height: 150 };
/** Edge settles a window we placed a moment later; events in between are its, not the user's */
const PLACEMENT_SETTLE_MS = 600;

enum MenuId {
  right = "splitPanes.right",
  down = "splitPanes.down",
  linkRight = "splitPanes.linkRight",
  linkDown = "splitPanes.linkDown"
}

const rectSchema = z.object({ left: z.number(), top: z.number(), width: z.number(), height: z.number() });
const nodeSchema: z.ZodType<Node> = z.lazy(() => z.union([
  z.object({ windowId: z.number() }),
  z.object({ axis: z.enum(Layout.Axis), children: z.array(nodeSchema), weights: z.array(z.number()) })
]));
const splitSchema = z.object({ anchorTabId: z.number(), hostWindowId: z.number(), root: nodeSchema });

/**
 * `splits`: one per split tab; the root's panes are the host window and the companions.
 * `regions`: each host's whole area while one of its splits is showing - what the host goes back to
 * when none is. `shown`: the anchor tab of the split each host is showing
 */
const stateSchema = z.object({
  splits: z.array(splitSchema),
  regions: z.record(z.string(), rectSchema),
  shown: z.record(z.string(), z.number())
});

type SplitTab = z.infer<typeof splitSchema>;
type State = z.infer<typeof stateSchema>;

const EMPTY_STATE: State = { splits: [], regions: {}, shown: {} };

/** Window and tab ids only mean something for one browser run; `onStartup` forgets them */
const stateItem = storage.defineItem<unknown>("local:splitPanesState");

let isEnabled = false;
let report = () => {};
let state: State = structuredClone(EMPTY_STATE);
/** The bounds each window was last given, so our own moves are not mistaken for the user's */
const placedBounds = new Map<number, Rect>();
const placedAt = new Map<number, number>();
const settleTimers = new Map<number, ReturnType<typeof setTimeout>>();
let raisedSplit: SplitTab | null = null;
let raiseQuietUntil = 0;

const loaded = load();

async function load() {
  const parsed = stateSchema.safeParse(await stateItem.getValue());
  if (!parsed.success) {
    return;
  }
  const openWindowIds = new Set((await browser.windows.getAll()).map(window => window.id));
  const splits = parsed.data.splits.flatMap(split => {
    let root: Node | null = split.root;
    for (const windowId of Layout.panes(split.root).filter(id => !openWindowIds.has(id))) {
      root = root && Layout.remove({ root, windowId });
    }
    const isStillSplit = root && !Layout.isPane(root) && openWindowIds.has(split.hostWindowId);
    return isStillSplit ? [{ ...split, root: root! }] : [];
  });
  state = { ...parsed.data, splits };
}

const save = () => stateItem.setValue(state);

const companions = (split: SplitTab) => Layout.panes(split.root).filter(id => id !== split.hostWindowId);

/** A showing split's host always has its area recorded; `startSplit` and `show` put it there */
const regionOf = (split: SplitTab) => state.regions[split.hostWindowId]!;

const isShowing = (split: SplitTab) => state.shown[split.hostWindowId] === split.anchorTabId;

const boundsOf = ({ left = 0, top = 0, width = 0, height = 0 }: Browser.windows.Window): Rect =>
  ({ left, top, width, height });

const isNear = (a: Rect, b: Rect) =>
  (["left", "top", "width", "height"] as const).every(key => Math.abs(a[key] - b[key]) <= BOUNDS_TOLERANCE);

/** For a host, the split it is showing; for a companion, its own split if that one is showing */
function shownSplitOf(windowId: number) {
  return state.splits.find(split => isShowing(split) && Layout.contains({ root: split.root, windowId }));
}

/** Where every pane of the split `windowId` belongs to sits right now, for moving focus between them */
export function shownPaneRects(windowId: number) {
  const split = shownSplitOf(windowId);
  if (!split) {
    return null;
  }
  return Layout.layout({ root: split.root, region: regionOf(split), gap: GAP });
}

/** The split an action on `window` means: a companion's own, or the one the host's active tab owns */
function splitOfWindow(window: Browser.windows.Window) {
  const companionOf = state.splits.find(split => companions(split).includes(window.id!));
  if (companionOf) {
    return companionOf;
  }
  const active = window.tabs?.find(tab => tab.active);
  return state.splits.find(split => split.anchorTabId === active?.id);
}

async function targetWindow(windowId?: number) {
  const window = windowId
    ? await browser.windows.get(windowId, { populate: true }).catch(() => null)
    : await browser.windows.getLastFocused({ populate: true, windowTypes: ["normal"] });
  const isSplittable = window?.type === "normal" && !window.incognito;
  return isSplittable ? window : null;
}

async function place(split: SplitTab) {
  const region = regionOf(split);
  const rects = Layout.layout({ root: split.root, region, gap: GAP });
  await Promise.all([...rects].map(([windowId, rect]) => {
    const bounds = Layout.toWindowBounds({ rect, region, border: INVISIBLE_BORDER });
    placedBounds.set(windowId, bounds);
    placedAt.set(windowId, Date.now());
    // `normal` brings a minimized or maximized pane back; no other state takes bounds
    return browser.windows.update(windowId, { state: "normal", ...bounds }).catch(() => {});
  }));
  report();
}

/** Splits always start from a host's active tab; the host's bounds become the split's area */
function startSplit(hostWindow: Browser.windows.Window) {
  const active = hostWindow.tabs!.find(tab => tab.active)!;
  const split: SplitTab = { anchorTabId: active.id!, hostWindowId: hostWindow.id!, root: Layout.pane(hostWindow.id!) };
  state.splits.push(split);
  state.regions[split.hostWindowId] ??= boundsOf(hostWindow);
  state.shown[split.hostWindowId] = split.anchorTabId;
  return split;
}

async function restoreHost(hostWindowId: number) {
  const region = state.regions[hostWindowId];
  delete state.regions[hostWindowId];
  if (!region) {
    return;
  }
  placedBounds.set(hostWindowId, region);
  await browser.windows.update(hostWindowId, { state: "normal", ...region }).catch(() => {});
}

/**
 * Folds a split back into its host: the companions' tabs land right after the tab that owned the
 * split, and the host takes its whole area again
 */
async function dissolve(split: SplitTab) {
  const wasShowing = isShowing(split);
  state.splits = state.splits.filter(other => other !== split);
  if (wasShowing) {
    delete state.shown[split.hostWindowId];
  }
  await save();
  const anchor = await browser.tabs.get(split.anchorTabId).catch(() => null);
  const isAnchorInHost = anchor?.windowId === split.hostWindowId;
  let index = isAnchorInHost ? anchor.index + 1 : -1;
  for (const companionId of companions(split)) {
    const tabs = await browser.tabs.query({ windowId: companionId });
    await browser.tabs.move(tabs.map(tab => tab.id!), { windowId: split.hostWindowId, index }).catch(() => {});
    if (index >= 0) {
      index += tabs.length;
    }
  }
  if (wasShowing) {
    await restoreHost(split.hostWindowId);
  }
  report();
}

/** The outgoing split's companions are minimized, the incoming one's tiled back into place */
async function show({ hostWindowId, split }: { hostWindowId: number; split: SplitTab | null }) {
  const shownAnchor = state.shown[hostWindowId];
  if (shownAnchor === split?.anchorTabId) {
    return;
  }
  const outgoing = state.splits.find(other => other.anchorTabId === shownAnchor);
  // Recorded before anything is awaited, so a tab switch arriving mid-way reads the new state
  if (split) {
    state.shown[hostWindowId] = split.anchorTabId;
  } else {
    delete state.shown[hostWindowId];
  }
  // Minimizing one pane hands focus to the next window, often a sibling about to be minimized
  // too; those focus changes are this switch's own, not the user picking a pane
  raiseQuietUntil = Date.now() + RAISE_QUIET_MS;
  if (outgoing) {
    await Promise.all(companions(outgoing).map(windowId =>
      browser.windows.update(windowId, { state: "minimized" }).catch(() => {})));
  }
  if (!split) {
    await restoreHost(hostWindowId);
  } else {
    if (!outgoing) {
      state.regions[hostWindowId] = boundsOf(await browser.windows.get(hostWindowId));
    }
    await place(split);
  }
  raiseQuietUntil = Date.now() + RAISE_QUIET_MS;
  await save();
}

/** The split an action on `window` would change, and its area, without starting one yet */
function planFor(window: Browser.windows.Window) {
  const split = splitOfWindow(window);
  if (split) {
    return { split, root: split.root, region: regionOf(split) };
  }
  return { split: null, root: Layout.pane(window.id!), region: boundsOf(window) };
}

const fits = ({ root, region }: { root: Node; region: Rect }) =>
  Layout.fits({ root, region, gap: GAP, minimum: MINIMUM_PANE });

const withPendingPane = ({ root, windowId, axis }: { root: Node; windowId: number; axis: Layout.Axis }) =>
  Layout.split({ root, targetWindowId: windowId, newWindowId: PENDING_PANE, axis });

const presetRoot = (template: Layout.Template) =>
  Layout.fromTemplate({ template, windowIds: Array.from({ length: Layout.slotCount(template) }, (_, slot) => -1 - slot) });

/** In a host, beside the active tab's split (starting one); in a companion, beside that pane */
async function addPane({ windowId, axis, url }: { windowId?: number; axis: Layout.Axis; url?: string }) {
  const window = await targetWindow(windowId);
  if (!window) {
    return;
  }
  const plan = planFor(window);
  const isRoomForPane = fits({ root: withPendingPane({ root: plan.root, windowId: window.id!, axis }), region: plan.region });
  if (!isRoomForPane) {
    return;
  }
  const split = plan.split ?? startSplit(window);
  const region = regionOf(split);
  const pendingRoot = withPendingPane({ root: split.root, windowId: window.id!, axis });
  const rect = Layout.layout({ root: pendingRoot, region, gap: GAP }).get(PENDING_PANE)!;
  const created = await browser.windows.create({
    type: "normal",
    focused: true,
    ...Layout.toWindowBounds({ rect, region, border: INVISIBLE_BORDER }),
    ...(url ? { url } : {})
  });
  split.root = Layout.replace({ root: pendingRoot, fromWindowId: PENDING_PANE, toWindowId: created!.id! });
  raisedSplit = split;
  await place(split);
  await save();
}

/**
 * The tab stays in the host; the tabs to its right fill the other panes, in order, and new tab
 * pages whatever is left
 */
export async function applyPreset({ windowId, presetId }: { windowId: number; presetId: PresetId }) {
  const preset = PRESETS.find(candidate => candidate.id === presetId);
  const target = await targetWindow(windowId);
  if (!preset || !target) {
    return;
  }
  const isRoomForPreset = fits({ root: presetRoot(preset.template), region: planFor(target).region });
  if (!isRoomForPreset) {
    return;
  }
  const existing = splitOfWindow(target);
  if (existing) {
    await dissolve(existing);
  }
  const window = await browser.windows.get(existing?.hostWindowId ?? target.id!, { populate: true });
  const split = startSplit(window);
  const region = regionOf(split);
  const active = window.tabs!.find(tab => tab.active)!;
  const followingTabs = window.tabs!.filter(tab => tab.index > active.index);
  const slots = Layout.slotCount(preset.template);
  const placeholderIds = Array.from({ length: slots }, (_, slot) => (slot === 0 ? window.id! : -1 - slot));
  const rects = Layout.layout({
    root: Layout.fromTemplate({ template: preset.template, windowIds: placeholderIds }),
    region,
    gap: GAP
  });

  const windowIds = [window.id!];
  for (let slot = 1; slot < slots; slot++) {
    const tab = followingTabs[slot - 1];
    const created = await browser.windows.create({
      type: "normal",
      focused: false,
      ...Layout.toWindowBounds({ rect: rects.get(placeholderIds[slot]!)!, region, border: INVISIBLE_BORDER }),
      ...(tab ? { tabId: tab.id } : {})
    });
    windowIds.push(created!.id!);
  }
  split.root = Layout.fromTemplate({ template: preset.template, windowIds });
  raisedSplit = split;
  await place(split);
  await browser.windows.update(window.id!, { focused: true });
  await save();
}

export async function merge(windowId?: number) {
  const window = await targetWindow(windowId);
  const split = window && splitOfWindow(window);
  if (split) {
    await dissolve(split);
  }
}

/** What the popup shows: the pane count, and which of its buttons Edge has room for */
export async function describe(windowId: number) {
  const window = await targetWindow(windowId);
  if (!window) {
    return { panes: 1, canSplitRight: false, canSplitDown: false, fittingPresetIds: [] };
  }
  const plan = planFor(window);
  const canSplit = (axis: Layout.Axis) =>
    fits({ root: withPendingPane({ root: plan.root, windowId: window.id!, axis }), region: plan.region });
  return {
    panes: Layout.panes(plan.root).length,
    canSplitRight: canSplit(Layout.Axis.row),
    canSplitDown: canSplit(Layout.Axis.column),
    fittingPresetIds: PRESETS.filter(preset => fits({ root: presetRoot(preset.template), region: plan.region }))
      .map(preset => preset.id)
  };
}

export const splitRight = (windowId?: number) => addPane({ windowId, axis: Layout.Axis.row });

export const splitDown = (windowId?: number) => addPane({ windowId, axis: Layout.Axis.column });

async function followUser(windowId: number) {
  const split = shownSplitOf(windowId);
  const window = await browser.windows.get(windowId).catch(() => null);
  const isFollowable = split && window && window.state !== "minimized";
  if (!isFollowable) {
    return;
  }
  const region = regionOf(split);
  // Maximizing any pane maximizes the grid: its maximized bounds become the split's area
  const isMaximized = window.state === "maximized" || window.state === "fullscreen";
  if (isMaximized) {
    state.regions[split.hostWindowId] = boundsOf(window);
  } else {
    const rect = Layout.layout({ root: split.root, region, gap: GAP }).get(windowId)!;
    const wanted = Layout.fromWindowBounds({ bounds: boundsOf(window), rect, region, border: INVISIBLE_BORDER });
    const next = Layout.resize({ root: split.root, region, gap: GAP, windowId, wanted, minimum: MINIMUM_PANE });
    // A drag that would squeeze a pane below what Edge allows snaps the grid back instead
    if (fits(next)) {
      split.root = next.root;
      state.regions[split.hostWindowId] = next.region;
    }
  }
  await place(split);
  await save();
}

async function onActivated({ tabId, windowId }: Browser.tabs.OnActivatedInfo) {
  await loaded;
  if (!isEnabled) {
    return;
  }
  const split = state.splits.find(candidate => candidate.anchorTabId === tabId);
  if (split?.hostWindowId === windowId) {
    await show({ hostWindowId: windowId, split });
    return;
  }
  const isHostShowingSomething = windowId in state.shown;
  if (isHostShowingSomething) {
    await show({ hostWindowId: windowId, split: null });
  }
}

/** A tab that owned a split, closed or dragged out, gives its panes back to the host's tab strip */
async function onAnchorGone(tabId: number) {
  await loaded;
  const split = state.splits.find(candidate => candidate.anchorTabId === tabId);
  if (split) {
    await dissolve(split);
  }
}

async function onBoundsChanged(window: Browser.windows.Window) {
  await loaded;
  const windowId = window.id!;
  if (!isEnabled || !shownSplitOf(windowId)) {
    return;
  }
  const placed = placedBounds.get(windowId);
  const isOurOwnMove = placed && isNear(placed, boundsOf(window));
  const isEdgeSettlingOurMove = Date.now() - (placedAt.get(windowId) ?? 0) < PLACEMENT_SETTLE_MS;
  if (isOurOwnMove || isEdgeSettlingOurMove) {
    return;
  }
  clearTimeout(settleTimers.get(windowId));
  settleTimers.set(windowId, setTimeout(() => followUser(windowId), SETTLE_MS));
}

async function onWindowRemoved(windowId: number) {
  await loaded;
  placedBounds.delete(windowId);
  placedAt.delete(windowId);
  // A host closing takes its tab strip with it, so its companions come back as ordinary windows
  // rather than staying minimized with nothing left to show them
  const hosted = state.splits.filter(split => split.hostWindowId === windowId);
  if (hosted.length > 0) {
    state.splits = state.splits.filter(split => split.hostWindowId !== windowId);
    delete state.regions[windowId];
    delete state.shown[windowId];
    await Promise.all(hosted.flatMap(companions).map(companionId =>
      browser.windows.update(companionId, { state: "normal" }).catch(() => {})));
    report();
    await save();
    return;
  }
  const split = state.splits.find(candidate => Layout.contains({ root: candidate.root, windowId }));
  if (!split) {
    return;
  }
  split.root = Layout.remove({ root: split.root, windowId })!;
  if (Layout.isPane(split.root)) {
    await dissolve(split);
    return;
  }
  if (isShowing(split)) {
    await place(split);
  }
  await save();
}

/**
 * Clicking one pane brings its whole grid forward, the way a window comes forward with all of its
 * tabs. A pane of a split that is not showing - restored from the taskbar, say - switches its host
 * to that tab
 */
async function onFocusChanged(windowId: number) {
  await loaded;
  const isOurOwnRaise = Date.now() < raiseQuietUntil;
  if (!isEnabled || isOurOwnRaise) {
    return;
  }
  const hiddenSplit = state.splits.find(split => companions(split).includes(windowId) && !isShowing(split));
  if (hiddenSplit) {
    // Windows can hand focus to a window on its way to being minimized; only one the user brought
    // back is a request to see its split
    const window = await browser.windows.get(windowId).catch(() => null);
    const isRestoredByUser = window?.state === "normal";
    if (isRestoredByUser) {
      await browser.tabs.update(hiddenSplit.anchorTabId, { active: true }).catch(() => {});
    }
    return;
  }
  const split = shownSplitOf(windowId);
  const isEnteringSplit = split && split !== raisedSplit;
  raisedSplit = split ?? null;
  if (!isEnteringSplit) {
    return;
  }
  raiseQuietUntil = Date.now() + RAISE_QUIET_MS;
  for (const otherId of Layout.panes(split.root).filter(id => id !== windowId)) {
    await browser.windows.update(otherId, { focused: true }).catch(() => {});
  }
  await browser.windows.update(windowId, { focused: true }).catch(() => {});
  raiseQuietUntil = Date.now() + RAISE_QUIET_MS;
}

/** Added once, when the background starts, so Edge can wake the worker for them */
export function listen() {
  browser.runtime.onStartup.addListener(() => stateItem.removeValue());
  browser.tabs.onActivated.addListener(onActivated);
  browser.tabs.onRemoved.addListener((tabId, { isWindowClosing }) => !isWindowClosing && onAnchorGone(tabId));
  browser.tabs.onDetached.addListener(tabId => onAnchorGone(tabId));
  browser.windows.onBoundsChanged.addListener(onBoundsChanged);
  browser.windows.onRemoved.addListener(onWindowRemoved);
  browser.windows.onFocusChanged.addListener(onFocusChanged);
}

export const splitPanes: FeatureModule = {
  async enable(context) {
    await loaded;
    report = context.report;
    isEnabled = true;
    await browser.contextMenus.removeAll();
    browser.contextMenus.create({ id: MenuId.right, title: "Split right", contexts: ["page", "frame"] });
    browser.contextMenus.create({ id: MenuId.down, title: "Split down", contexts: ["page", "frame"] });
    browser.contextMenus.create({ id: MenuId.linkRight, title: "Open link in a pane to the right", contexts: ["link"] });
    browser.contextMenus.create({ id: MenuId.linkDown, title: "Open link in a pane below", contexts: ["link"] });
    report();
  },
  // Every split folds back into its tab strip, so no tab is left hiding in a minimized window
  async disable() {
    isEnabled = false;
    for (const split of [...state.splits]) {
      await dissolve(split);
    }
    await Promise.all(Object.values(MenuId).map(id => browser.contextMenus.remove(id).catch(() => {})));
  },
  status() {
    let panes = 0;
    for (const split of state.splits) {
      panes += Layout.panes(split.root).length;
    }
    return { splits: state.splits.length, panes };
  },
  commands: {
    [Command.splitRight]: splitRight,
    [Command.splitDown]: splitDown,
    [Command.unsplit]: merge
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
