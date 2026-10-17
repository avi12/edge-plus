import type { AppState, EdgeUpdate } from "@/lib/bridge";
import { Feature } from "@/lib/bridge";

// The sentences the panel shows when Edge runs a build the split DLL was not
// made for: once when an update switches features off, and for as long as
// that build keeps them held off.

// How a feature is named in a sentence.
const featureNames: Record<Feature, string> = {
  [Feature.NativeSplit]: "Split view",
  [Feature.Theming]: "Theming",
  [Feature.QuietDevMode]: "Hiding the developer-mode prompt",
  [Feature.WebHandlers]: "Gmail and Google Calendar links"
};

function joinNames(features: Feature[]): string {
  return features.map(feature => featureNames[feature]).join(" and ");
}

/** What an Edge update switched off, as one sentence; empty when it switched nothing off. */
export function describeUpdate(update: EdgeUpdate): string {
  if (update.disabled.length === 0) {
    return "";
  }

  const isUpdate = update.previousBuild !== null && update.previousBuild !== update.edgeBuild;
  const lead = isUpdate ? `Edge updated to ${update.edgeBuild}` : `Edge ${update.edgeBuild} is running`;
  return `${lead}; turned off ${joinNames(update.disabled)} — Fix Edge supports Edge ${update.supportedBuild}.`;
}

/** Which features the running Edge holds off, as one sentence; empty when it holds none. */
export function describeUnsupported(state: AppState): string {
  if (state.unsupported.length === 0) {
    return "";
  }

  return `${joinNames(state.unsupported)} need Edge ${state.supportedEdgeBuild}; ` +
    `this is Edge ${state.edgeBuild ?? "unknown"}.`;
}
