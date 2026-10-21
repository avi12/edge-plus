using System;
using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// A newly seen Edge build, and what it switched off (often nothing).
internal sealed record BuildChange(
    FileVersion EdgeBuild,
    FileVersion? PreviousBuild,
    FileVersion SupportedBuild,
    IReadOnlyList<Feature> Disabled);

// Which Edge build the DLL is made for, and which features another build rules
// out. Pure: what is running is handed in (EdgeWatch reads it).
//
// The split DLL is pinned to one msedge.dll build: every RVA and struct offset
// in app/native-split/src/offsets.h belongs to it. The build comes from
// offsets.h itself (Shared), so the panel compares against exactly what the
// embedded DLL was reversed against — there is no second copy to drift.
//
// What each feature depends on:
//
// * The split, the theming, the link handlers, the find bar, the WebUI theme
//   colours, the live preview, the tab spine and the reading theme all run
//   inside the injected DLL and call into
//   msedge.dll by RVA, so a different msedge.dll breaks every one of them.
// * The developer-mode prompt only edits a key in each profile's Preferences
//   (DevModePrompt) and touches no Edge binary, so an update leaves it alone.
internal static class EdgeBuild {
  public static FileVersion Pinned {
    get;
  } = new(
      Shared.EdgeVersionMajor, Shared.EdgeVersionMinor, Shared.EdgeVersionBuild,
      Shared.EdgeVersionRevision);

  public static bool DependsOnPinnedBuild(Feature feature) => feature switch {
    Feature.NativeSplit or Feature.Theming or Feature.ProtocolHandlers or Feature.EnhancedFind
        or Feature.WebUiFollowsTheme or Feature.LivePreview or Feature.TabSpine
        or Feature.ReadingTheme => true,
    Feature.QuietDevMode => false,
    _ => throw new ArgumentOutOfRangeException(nameof(feature)),
  };

  // The build running Edge is judged by, given each browser process's build:
  // any build other than the pinned one if a process runs one (the DLL must not
  // go into it), else the pinned one; null when none could be read (Edge is not
  // running).
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

  // The features that cannot run on `running`, on or off.
  public static IReadOnlyList<Feature> Unsupported(FileVersion running, FileVersion pinned) {
    if (running == pinned) {
      return [];
    }
    return Enum.GetValues<Feature>().Where(DependsOnPinnedBuild).ToArray();
  }

  // The features that cannot run on `running` and are on in `features`.
  public static IReadOnlyList<Feature> Blocked(Features features, FileVersion running,
                                               FileVersion pinned) =>
      Unsupported(running, pinned).Where(features.IsOn).ToArray();
}
