using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// Read fresh after every action.
internal sealed record PanelState(
    Features Features,
    Theming Theming,
    // Themes installed in Edge that the panel did not add.
    IReadOnlyList<InstalledTheme> EdgeThemes,
    bool IsEdgeRunning,
    bool IsStartWithWindows,
    bool IsSplitLoaded,
    // The running build, else the installed one; null when it cannot be read.
    FileVersion? EdgeBuild,
    FileVersion SupportedEdgeBuild) {
  public FeatureState StateOf(Feature feature) =>
      EdgeSupport.StateOf(Features, feature, EdgeBuild, SupportedEdgeBuild);

  // A paused or unavailable switch is held as it is until a supported build.
  public bool IsLocked(Feature feature) =>
      StateOf(feature) is FeatureState.Paused or FeatureState.Unavailable;

  public IReadOnlyList<Feature> Paused =>
      EdgeSupport.Paused(Features, EdgeBuild, SupportedEdgeBuild);
}
