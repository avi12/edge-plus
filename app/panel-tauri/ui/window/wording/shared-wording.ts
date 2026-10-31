import shared from "./shared-wording.json" with { type: "json" };
import type { EdgeBuild, Feature, SwitchKey } from "#src/protocol.g.ts";

const EDGE_PLACEHOLDER = "{edge}";
const LEAD_PLACEHOLDER = "{lead}";
const TITLES_PLACEHOLDER = "{titles}";

export const featureTitles: Readonly<Record<Feature, string>> = shared.featureTitles;

function isFeature(key: SwitchKey): key is Feature {
  return Object.hasOwn(shared.featureTitles, key);
}

export function featureTitle(key: SwitchKey) {
  return isFeature(key) ? shared.featureTitles[key] : undefined;
}

export function edgeLead(edge: EdgeBuild) {
  const template = edge.isUpdated ? shared.edgeUpdated : shared.edgeOnVersion;
  return template.replace(EDGE_PLACEHOLDER, edge.version);
}

export function pausedNotice({ edge, paused }: {
  edge: EdgeBuild | null;
  paused: Feature[];
}) {
  const hasPausedFeatures = edge !== null && paused.length > 0;
  if (!hasPausedFeatures) {
    return "";
  }

  const titles = paused.map(feature => shared.featureTitles[feature]).join(shared.titleSeparator);
  return shared.pausedUntilSupported.replace(LEAD_PLACEHOLDER, edgeLead(edge)).replace(TITLES_PLACEHOLDER, titles);
}
