using System;
using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

internal enum FeatureState {
  Off,
  On,
  // Switched on, but held on an unsupported build; comes back by itself on a
  // supported build.
  Paused,
  // Switched off, and could not be switched on on this build.
  Unavailable,
}

// The pause is derived from the build every time and the user's choice is never
// rewritten, so the choice comes back by itself. The check is the whole build,
// not per feature: the DLL (verify.h) refuses anything but the exact msedge.dll
// offsets.h was reversed against.
internal static class EdgeSupport {
  // An unknown build (Edge not installed or unreadable) holds nothing back.
  public static bool IsSupported(FileVersion? edge, FileVersion pinned) =>
      edge is not FileVersion build || build == pinned;

  public static FeatureState StateOf(Features choice, Feature feature, FileVersion? edge,
                                     FileVersion pinned) {
    bool isOn = choice.IsOn(feature);
    bool isHeld = EdgeBuild.DependsOnPinnedBuild(feature) && !IsSupported(edge, pinned);
    if (!isHeld) {
      return isOn ? FeatureState.On : FeatureState.Off;
    }
    return isOn ? FeatureState.Paused : FeatureState.Unavailable;
  }

  public static IReadOnlyList<Feature> Paused(Features choice, FileVersion? edge,
                                              FileVersion pinned) =>
      Enum.GetValues<Feature>()
          .Where(feature => StateOf(choice, feature, edge, pinned) == FeatureState.Paused)
          .ToArray();

  // What the DLL is sent, never what is saved.
  public static Features Effective(Features choice, FileVersion? edge, FileVersion pinned) {
    var effective = new Features();
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      effective.Set(feature, StateOf(choice, feature, edge, pinned) == FeatureState.On);
    }
    return effective;
  }

  // One toast per build that pauses something.
  public static bool ShouldNotify(IReadOnlyList<Feature> paused, FileVersion? edge,
                                  FileVersion? notifiedFor) =>
      paused.Count > 0 && edge is FileVersion build && build != notifiedFor;
}
