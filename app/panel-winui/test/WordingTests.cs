using System;

namespace EdgePlus;

internal static class WordingTests {
  private static readonly FileVersion s_supported = new(155, 0, 4283, 45);
  private static readonly FileVersion s_updated = new(156, 0, 1, 0);

  public static void Run() {
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      CardWording wording = Wording.For(feature);
      Expect.That(wording.Title.Length > 0 && wording.Description.Length > 0,
                  $"{feature} has words");
    }
    Expect.Equal("Enhanced find: RegExp and Ask Copilot", Wording.For(Feature.EnhancedFind).Title,
                 "the find title names the assistant");
    Expect.Equal("Light blue", Wording.ReadingThemeLabel(ReadingTheme.LightBlue),
                 "Edge's LightBlue reads as Light blue");
    Expect.Equal("Dark grey", Wording.ReadingThemeLabel(ReadingTheme.DarkGrey),
                 "Edge's DarkGrey reads as Dark grey");
    Expect.Equal("Sepia", Wording.ReadingThemeLabel(ReadingTheme.Sepia), "one word stays");
    Expect.Equal(12, ReadingThemes.Grid.Length, "the grid has twelve themes");
    Expect.That(ReadingThemes.Grid[0] == ReadingTheme.Sepia
                    && ReadingThemes.Grid[5] == ReadingTheme.Green
                    && ReadingThemes.Grid[6] == ReadingTheme.Lavender
                    && ReadingThemes.Grid[11] == ReadingTheme.DarkGrey,
                "in Edge's grid order: Sepia..Green, then Lavender..DarkGrey");

    var choice = new Features {
      NativeSplit = true,
      Theming = true,
      LivePreview = false,
      TabSpine = false,
    };
    PanelState updated = State(choice, s_updated);
    Expect.Equal("Edge updated to 156.0.1.0. Split into any grid is paused until Edge+ supports " +
                     "this version.",
                 Wording.DescribeFeature(updated, Feature.NativeSplit), "a paused card says so");
    Expect.Equal("Edge updated to 156.0.1.0. Live tab preview on hover is unavailable until " +
                     "Edge+ supports this version.",
                 Wording.DescribeFeature(updated, Feature.LivePreview), "an off one is unavailable");
    Expect.Equal("", Wording.DescribeFeature(updated, Feature.QuietDevMode),
                 "a panel-only feature says nothing");
    Expect.That(updated.IsLocked(Feature.NativeSplit) && !updated.IsLocked(Feature.QuietDevMode),
                "a paused switch is locked, a panel-only one is not");
    Expect.Equal("", Wording.DescribeFeature(State(choice, s_supported), Feature.NativeSplit),
                 "the supported build says nothing");
    Expect.Equal("Edge is on version 154.0.1.0.",
                 Wording.EdgeLead(new FileVersion(154, 0, 1, 0), s_supported),
                 "an older build is not called an update");

    Expect.Equal("Edge updated to 156.0.1.0. Paused until Edge+ supports this version: Split " +
                     "into any grid and Theme by Windows light and dark.",
                 Wording.DescribePaused(s_updated, s_supported, updated.Paused),
                 "the summary names each paused feature by its card");
    Expect.Equal("", Wording.DescribePaused(s_updated, s_supported, []), "nothing paused");
  }

  private static PanelState State(Features choice, FileVersion? edge) =>
      new(choice, new Theming(), new ReadingMode(), [], IsEdgeRunning: edge is not null,
          IsStartWithWindows: false, IsSplitLoaded: false, edge, s_supported);
}
