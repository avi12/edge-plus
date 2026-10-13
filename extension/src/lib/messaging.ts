import type { PresetId } from "@/features/split-panes/presets";
import { defineExtensionMessaging } from "@webext-core/messaging";

/** The values are the wire contract between the popup and the background */
export enum MessageType {
  describeSplit = "describeSplit",
  splitRight = "splitRight",
  splitDown = "splitDown",
  applyPreset = "applyPreset",
  addSelectedTabs = "addSelectedTabs",
  tile = "tile",
  merge = "merge"
}

/** `isEnabled` false means the feature is switched off in the Fix Edge app */
type SplitDescription = {
  isEnabled: boolean;
  panes: number;
  /** Tabs selected in the tab strip that could become panes, the current one included */
  selectedTabs: number;
};

type ProtocolMap = {
  [MessageType.describeSplit](windowId: number): SplitDescription;
  [MessageType.splitRight](windowId: number): void;
  [MessageType.splitDown](windowId: number): void;
  [MessageType.applyPreset](request: { windowId: number; presetId: PresetId }): void;
  [MessageType.merge](windowId: number): void;
  [MessageType.addSelectedTabs](windowId: number): void;
  [MessageType.tile](windowId: number): void;
};

export const { sendMessage, onMessage } = defineExtensionMessaging<ProtocolMap>();
