import type { PresetId } from "@/features/split-panes/presets";
import { defineExtensionMessaging } from "@webext-core/messaging";

/** The values are the wire contract between the popup and the background */
export enum MessageType {
  describeSplit = "describeSplit",
  splitRight = "splitRight",
  splitDown = "splitDown",
  applyPreset = "applyPreset",
  merge = "merge"
}

/**
 * `isEnabled` false means the feature is switched off in the Fix Edge app. The `can`s and the
 * presets are what Edge has room for, since it will not make a window narrower than about 500px
 */
type SplitDescription = {
  isEnabled: boolean;
  panes: number;
  canSplitRight: boolean;
  canSplitDown: boolean;
  fittingPresetIds: PresetId[];
};

type ProtocolMap = {
  [MessageType.describeSplit](windowId: number): SplitDescription;
  [MessageType.splitRight](windowId: number): void;
  [MessageType.splitDown](windowId: number): void;
  [MessageType.applyPreset](request: { windowId: number; presetId: PresetId }): void;
  [MessageType.merge](windowId: number): void;
};

export const { sendMessage, onMessage } = defineExtensionMessaging<ProtocolMap>();
