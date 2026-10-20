using System;

namespace EdgePlus;

internal static class WordingTests {
  private static readonly FileVersion s_supported = new(155, 0, 4283, 45);
  private static readonly FileVersion s_running = new(156, 0, 1, 0);

  public static void Run() {
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      CardWording wording = Wording.For(feature);
      Expect.That(wording.Title.Length > 0 && wording.Description.Length > 0,
                  $"{feature} has words");
      Expect.That(Wording.NameInSentence(feature).Length > 0, $"{feature} has a name");
    }
    Expect.Equal("Enhanced find: RegExp and Ask Copilot", Wording.For(Feature.EnhancedFind).Title,
                 "the find title names the assistant");

    var nothingOff = new BuildChange(s_running, null, s_supported, []);
    Expect.Equal("", Wording.DescribeUpdate(nothingOff), "a change that turned nothing off");
    var update = new BuildChange(s_running, s_supported, s_supported,
                                 [Feature.NativeSplit, Feature.Theming]);
    Expect.Equal("Edge updated to 156.0.1.0; turned off Split view and Theming — Edge+ " +
                     "supports Edge 155.0.4283.45.",
                 Wording.DescribeUpdate(update), "an update");
    var firstSeen = new BuildChange(s_running, null, s_supported, [Feature.Theming]);
    Expect.Equal("Edge 156.0.1.0 is running; turned off Theming — Edge+ supports Edge " +
                     "155.0.4283.45.",
                 Wording.DescribeUpdate(firstSeen), "a build seen first");

    PanelState held = State(running: s_running, unsupported: [Feature.LivePreview]);
    Expect.Equal("Unavailable on Edge 156.0.1.0 — Edge+ supports Edge 155.0.4283.45.",
                 Wording.DescribeHeldOff(held), "why a card is locked");
    Expect.Equal("Live tab preview on hover need Edge 155.0.4283.45; this is Edge 156.0.1.0.",
                 Wording.DescribeUnsupported(held), "what the running Edge holds off");
    Expect.That(held.IsHeldOff(Feature.LivePreview), "an unsupported switch that is off is held");
    Expect.That(!held.IsHeldOff(Feature.NativeSplit), "a supported switch is not held");
    Expect.Equal("", Wording.DescribeUnsupported(State(null, [])), "nothing held off");
  }

  private static PanelState State(FileVersion? running, Feature[] unsupported) =>
      new(new Features(), new Theming(), [], IsEdgeRunning: running is not null,
          IsStartWithWindows: false, IsSplitLoaded: false, running, s_supported, unsupported);
}
