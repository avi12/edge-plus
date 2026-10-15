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
  // Use one theme in Windows' light mode and another in dark, switched live.
  // Rides the same injected DLL as the split; its own switch. The theme library
  // and light/dark assignment UI that give this switch its effect come next.
  Theming,
}

internal static class Features {
  // The id as the settings file keys it.
  public static string WireName(this FeatureId feature) => feature switch {
    FeatureId.QuietDevMode => "quietDevMode",
    FeatureId.NativeSplit => "nativeSplit",
    FeatureId.Theming => "enhancedTheming",
    _ => throw new ArgumentOutOfRangeException(nameof(feature)),
  };
}
