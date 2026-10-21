using System;
using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// Where one feature stands, given the user's choice and the Edge build.
internal enum FeatureState {
  // Switched off by the user.
  Off,
  // Switched on, and running.
  On,
  // Switched on by the user, but held while Edge runs a build Edge+ does not
  // support; it comes back by itself when a supported build (or an Edge+
  // update that supports this one) arrives.
  Paused,
  // Switched off, and could not be switched on on this build.
  Unavailable,
}

// What a different Edge build does to each feature, kept apart from the user's
// on/off choice: the choice is never rewritten, the pause is derived from the
// build every time, so the choice comes back by itself. Pure, so it is unit
// tested.
//
// The check is the whole build, not per feature or per hook site: the DLL
// (verify.h) refuses anything but the exact msedge.dll offsets.h was reversed
// against, and every hook site is an address into that one build, so a new
// build moves them all.
internal static class EdgeSupport {
  // Whether `edge` is a build the DLL supports; an unknown build (Edge not
  // installed or unreadable) holds nothing back.
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

  // The features the user switched on that this build pauses.
  public static IReadOnlyList<Feature> Paused(Features choice, FileVersion? edge,
                                              FileVersion pinned) =>
      Enum.GetValues<Feature>()
          .Where(feature => StateOf(choice, feature, edge, pinned) == FeatureState.Paused)
          .ToArray();

  // The switches as they actually apply: the user's choice with every paused
  // feature off. What the DLL is sent, never what is saved.
  public static Features Effective(Features choice, FileVersion? edge, FileVersion pinned) {
    var effective = new Features();
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      effective.Set(feature, StateOf(choice, feature, edge, pinned) == FeatureState.On);
    }
    return effective;
  }

  // Whether to tell the user (one toast) about the features `edge` pauses: only
  // when it pauses some, and only once per build.
  public static bool ShouldNotify(IReadOnlyList<Feature> paused, FileVersion? edge,
                                  FileVersion? notifiedFor) =>
      paused.Count > 0 && edge is FileVersion build && build != notifiedFor;
}
