using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// The whole state the window renders, read fresh after every action.
internal sealed record PanelState(
    Features Features,
    Theming Theming,
    // Themes Edge already has installed that the panel did not add; applied by
    // id like the panel's own.
    IReadOnlyList<InstalledTheme> EdgeThemes,
    bool IsEdgeRunning,
    // Windows will start the panel at sign-in; read from the Run key itself.
    bool IsStartWithWindows,
    bool IsSplitLoaded,
    // The Edge build now (EdgeWatch.CurrentBuild: the running one, else the
    // installed one); null when it cannot be read.
    FileVersion? EdgeBuild,
    // The only build the split DLL supports.
    FileVersion SupportedEdgeBuild) {
  public FeatureState StateOf(Feature feature) =>
      EdgeSupport.StateOf(Features, feature, EdgeBuild, SupportedEdgeBuild);

  // A paused or unavailable switch is held as it is until a supported build.
  public bool IsLocked(Feature feature) =>
      StateOf(feature) is FeatureState.Paused or FeatureState.Unavailable;

  // The user's features this Edge build pauses.
  public IReadOnlyList<Feature> Paused =>
      EdgeSupport.Paused(Features, EdgeBuild, SupportedEdgeBuild);
}
