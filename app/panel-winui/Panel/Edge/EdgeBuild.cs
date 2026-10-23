using System;
using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

internal sealed record BuildChange(
    FileVersion EdgeBuild,
    FileVersion? PreviousBuild,
    FileVersion SupportedBuild,
    IReadOnlyList<Feature> Paused);

// The pinned build comes from offsets.h itself (via Shared), so the panel
// compares against exactly what the embedded DLL was reversed against. Every
// DLL feature calls into msedge.dll by RVA, so any other build breaks them all;
// the developer-mode prompt only edits Preferences and survives an update.
internal static class EdgeBuild {
  public static FileVersion Pinned {
    get;
  } = new(
      Shared.EdgeVersionMajor, Shared.EdgeVersionMinor, Shared.EdgeVersionBuild,
      Shared.EdgeVersionRevision);

  public static bool DependsOnPinnedBuild(Feature feature) => feature switch {
    Feature.NativeSplit or Feature.Theming or Feature.ProtocolHandlers or Feature.EnhancedFind
        or Feature.WebUiFollowsTheme or Feature.LivePreview or Feature.TabShape
        or Feature.ReadingTheme => true,
    Feature.QuietDevMode => false,
    _ => throw new ArgumentOutOfRangeException(nameof(feature)),
  };

  // Any process on a build other than the pinned one decides (the DLL must not
  // go into it); null when none could be read (Edge is not running).
  public static FileVersion? Judged(IReadOnlyList<FileVersion> builds, FileVersion pinned) {
    if (builds.Count == 0) {
      return null;
    }
    foreach (FileVersion build in builds) {
      if (build != pinned) {
        return build;
      }
    }
    return builds[0];
  }
}
