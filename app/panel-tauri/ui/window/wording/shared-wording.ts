import type { EdgeBuild, Feature, SwitchKey } from "../../protocol.g.ts";
import shared from "./shared-wording.json" with { type: "json" };

// The words the window shares with the core's paused toast (wording.rs reads
// the same file): every feature's title and the sentence naming the paused
// ones. Everything else the window says is in its markup.

const edgePlaceholder = "{edge}";
const leadPlaceholder = "{lead}";
const titlesPlaceholder = "{titles}";

function isFeature(key: SwitchKey): key is Feature {
  return Object.hasOwn(shared.featureTitles, key);
}

export function featureTitle(key: SwitchKey): string | undefined {
  return isFeature(key) ? shared.featureTitles[key] : undefined;
}

// "Edge updated to …." or "Edge is on version …."
export function edgeLead(edge: EdgeBuild): string {
  const template = edge.isUpdated ? shared.edgeUpdated : shared.edgeOnVersion;
  return template.replace(edgePlaceholder, edge.version);
}

export function pausedNotice(edge: EdgeBuild | null, paused: Feature[]): string {
  const hasPausedFeatures = edge !== null && paused.length > 0;
  if (!hasPausedFeatures) {
    return "";
  }

  const titles = paused.map(feature => shared.featureTitles[feature]).join(shared.titleSeparator);
  return shared.pausedUntilSupported.replace(leadPlaceholder, edgeLead(edge)).replace(titlesPlaceholder, titles);
}
