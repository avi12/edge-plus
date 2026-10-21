using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// The whole state the window renders, read fresh after every action.
internal sealed record PanelState(
    Features Features,
    Theming Theming,
    ReadingMode ReadingMode,
    // Themes Edge already has installed that the panel did not add; applied by
    // id like the panel's own.
    IReadOnlyList<InstalledTheme> EdgeThemes,
    bool IsEdgeRunning,
    // Windows will start the panel at sign-in; read from the Run key itself.
    bool IsStartWithWindows,
    bool IsSplitLoaded,
    // The msedge.dll build the running Edge is on; null when Edge is not running.
    FileVersion? EdgeBuild,
    // The only build the split DLL supports.
    FileVersion SupportedEdgeBuild,
    // The features that cannot run on the running build; their switches are held
    // off until Edge is on the supported build.
    IReadOnlyList<Feature> Unsupported) {
  // A held-off switch can still be turned off, never on.
  public bool IsHeldOff(Feature feature) =>
      Unsupported.Contains(feature) && !Features.IsOn(feature);
}
