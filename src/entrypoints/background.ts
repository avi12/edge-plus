import { FEATURES, FeatureId } from "@/features/catalog";
import * as SplitPanes from "@/features/split-panes/split-panes";
import type { Command, FeatureModule } from "@/lib/feature";
import type { Settings } from "@/lib/host";
import { connectHost, reportToHost, settingsSchema } from "@/lib/host";
import { MessageType, onMessage } from "@/lib/messaging";
import { defineBackground, storage } from "#imports";

const MODULES: Record<FeatureId, FeatureModule> = {
  [FeatureId.splitPanes]: SplitPanes.splitPanes
};

/** The last switches the app sent, so they hold while the app is closed */
const settingsItem = storage.defineItem<unknown>("local:settings");

export default defineBackground(() => {
  const activeFeatures = new Set<FeatureId>();

  function report() {
    const statusByFeature: Record<string, Record<string, number>> = {};
    for (const id of activeFeatures) {
      statusByFeature[id] = MODULES[id].status?.() ?? {};
    }
    reportToHost(statusByFeature);
  }

  async function applySettings(settings: Settings) {
    for (const feature of FEATURES) {
      const isWanted = settings.features?.[feature.id]?.enabled ?? feature.enabledByDefault;
      const isActive = activeFeatures.has(feature.id);
      if (isWanted && !isActive) {
        activeFeatures.add(feature.id);
        await MODULES[feature.id].enable({ report });
      } else if (!isWanted && isActive) {
        activeFeatures.delete(feature.id);
        await MODULES[feature.id].disable();
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
    for (const id of activeFeatures) {
      await MODULES[id].commands?.[command as Command]?.(tab?.windowId);
    }
  });

  browser.contextMenus.onClicked.addListener(async (info, tab) => {
    await ready;
    const featureId = String(info.menuItemId).split(".")[0] as FeatureId;
    if (activeFeatures.has(featureId)) {
      await MODULES[featureId].onMenu?.(info, tab);
    }
  });

  async function isSplitEnabled() {
    await ready;
    return activeFeatures.has(FeatureId.splitPanes);
  }

  onMessage(MessageType.describeSplit, async ({ data: windowId }) => {
    if (!await isSplitEnabled()) {
      return { isEnabled: false, panes: 1, canSplitRight: false, canSplitDown: false, fittingPresetIds: [] };
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

  onMessage(MessageType.merge, async ({ data: windowId }) => {
    if (await isSplitEnabled()) {
      await SplitPanes.merge(windowId);
    }
  });
});
