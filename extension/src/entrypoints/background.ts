import { FEATURES, FeatureId } from "@/features/catalog";
import { paneFocus } from "@/features/pane-focus/pane-focus";
import * as SplitPanes from "@/features/split-panes/split-panes";
import type { Command, FeatureModule } from "@/lib/feature";
import type { Settings } from "@/lib/host";
import { connectHost, reportToHost, settingsSchema } from "@/lib/host";
import { MessageType, onMessage } from "@/lib/messaging";
import { defineBackground, storage } from "#imports";

/** A feature with no module here runs entirely in the app, like hiding the developer-mode prompt */
const MODULES: Partial<Record<FeatureId, FeatureModule>> = {
  [FeatureId.splitPanes]: SplitPanes.splitPanes,
  [FeatureId.paneFocus]: paneFocus
};

const EXTENSION_FEATURES = FEATURES.flatMap(feature => {
  const module = MODULES[feature.id];
  return module ? [{ ...feature, module }] : [];
});

/** The last switches the app sent, so they hold while the app is closed */
const settingsItem = storage.defineItem<unknown>("local:settings");

export default defineBackground(() => {
  const activeModules = new Map<FeatureId, FeatureModule>();

  function report() {
    const statusByFeature: Record<string, Record<string, number>> = {};
    for (const [id, module] of activeModules) {
      statusByFeature[id] = module.status?.() ?? {};
    }
    reportToHost(statusByFeature);
  }

  async function applySettings(settings: Settings) {
    for (const feature of EXTENSION_FEATURES) {
      const isWanted = settings.features?.[feature.id]?.enabled ?? feature.enabledByDefault;
      const isActive = activeModules.has(feature.id);
      const isSwitchingOn = isWanted && !isActive;
      if (isSwitchingOn) {
        activeModules.set(feature.id, feature.module);
        await feature.module.enable({ report });
        continue;
      }
      const isSwitchingOff = !isWanted && isActive;
      if (isSwitchingOff) {
        activeModules.delete(feature.id);
        await feature.module.disable();
      }
    }
    report();
  }

  const ready = (async () => {
    const parsed = settingsSchema.safeParse(await settingsItem.getValue());
    await applySettings(parsed.success ? parsed.data : {});
    connectHost({
      onSettings: async settings => {
        await settingsItem.setValue(settings);
        await applySettings(settings);
      }
    });
  })();

  // Every listener is added synchronously, so Edge can wake the worker for it
  SplitPanes.listen();

  browser.commands.onCommand.addListener(async (command, tab) => {
    await ready;
    for (const module of activeModules.values()) {
      await module.commands?.[command as Command]?.(tab?.windowId);
    }
  });

  browser.contextMenus.onClicked.addListener(async (info, tab) => {
    await ready;
    const featureId = String(info.menuItemId).split(".")[0] as FeatureId;
    await activeModules.get(featureId)?.onMenu?.(info, tab);
  });

  async function isSplitEnabled() {
    await ready;
    return activeModules.has(FeatureId.splitPanes);
  }

  onMessage(MessageType.describeSplit, async ({ data: windowId }) => {
    if (!await isSplitEnabled()) {
      return { isEnabled: false, panes: 1, selectedTabs: 0 };
    }
    return { isEnabled: true, ...await SplitPanes.describe(windowId) };
  });

  onMessage(MessageType.splitRight, async ({ data: windowId }) => {
    if (await isSplitEnabled()) {
      await SplitPanes.splitRight(windowId);
    }
  });

  onMessage(MessageType.splitDown, async ({ data: windowId }) => {
    if (await isSplitEnabled()) {
      await SplitPanes.splitDown(windowId);
    }
  });

  onMessage(MessageType.applyPreset, async ({ data }) => {
    if (await isSplitEnabled()) {
      await SplitPanes.applyPreset(data);
    }
  });

  onMessage(MessageType.addSelectedTabs, async ({ data: windowId }) => {
    if (await isSplitEnabled()) {
      await SplitPanes.addSelectedTabs(windowId);
    }
  });

  onMessage(MessageType.tile, async ({ data: windowId }) => {
    if (await isSplitEnabled()) {
      await SplitPanes.tile(windowId);
    }
  });

  onMessage(MessageType.merge, async ({ data: windowId }) => {
    if (await isSplitEnabled()) {
      await SplitPanes.merge(windowId);
    }
  });
});
