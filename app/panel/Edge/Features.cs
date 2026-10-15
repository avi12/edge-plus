using System;

namespace FixEdge;

// The features the panel manages, now that everything lives in the app and the
// injected DLL — no extension, no catalog. Each is its own switch with its own
// persisted flag (FeatureSettings).
internal enum FeatureId {
  // Hide Edge's developer-mode prompt (DevModePrompt).
  QuietDevMode,
  // The N-pane native split DLL, injected into Edge (NativeSplit).
  NativeSplit,
}

internal static class Features {
  // The id as the settings file keys it.
  public static string WireName(this FeatureId feature) => feature switch {
    FeatureId.QuietDevMode => "quietDevMode",
    FeatureId.NativeSplit => "nativeSplit",
    _ => throw new ArgumentOutOfRangeException(nameof(feature)),
  };
}
