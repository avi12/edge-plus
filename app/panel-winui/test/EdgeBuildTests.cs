using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace EdgePlus;

internal static class EdgeBuildTests {
  private static readonly FileVersion s_pinned = Version("155.0.4283.45");

  private static readonly Feature[] s_dllFeatures = [
    Feature.NativeSplit, Feature.Theming, Feature.ProtocolHandlers, Feature.EnhancedFind,
    Feature.WebUiFollowsTheme, Feature.LivePreview,
  ];

  public static void Run() {
    ParsesFourPartVersionsOnly();
    OrdersByPart();
    ReadsASystemBinary();
    ThePinnedBuildComesFromOffsetsH();
    OnlyTheDllFeaturesDependOnTheBuild();
    ThePinnedBuildBlocksNothing();
    AnUpdateBlocksTheDllFeaturesOnly();
    ADowngradeIsAMismatchToo();
    FeaturesAlreadyOffAreNotReported();
    AMismatchedProcessDecidesTheRunningBuild();
  }

  private static FileVersion Version(string text) =>
      FileVersion.Parse(text) ?? throw new CheckFailedException($"{text} should parse");

  private static void ParsesFourPartVersionsOnly() {
    Expect.Equal(new FileVersion(155, 0, 4283, 45), Version("155.0.4283.45"), "four parts");
    Expect.Equal("155.0.4283.45", Version(" 155.0.4283.45 ").ToString(), "round trip");
    foreach (string text in new[] { "", "155", "155.0.4283", "155.0.4283.45.1", "155.0.x.45",
                                    "155.0.4283.70000" }) {
      Expect.That(FileVersion.Parse(text) is null, $"\"{text}\" does not parse");
    }
  }

  private static void OrdersByPart() {
    Expect.That(Version("155.0.4283.45") < Version("155.0.4284.2"), "build outranks revision");
    Expect.That(Version("156.0.0.0") > Version("155.9.9999.9"), "major outranks the rest");
  }

  private static void ReadsASystemBinary() {
    string kernel32 = Path.Combine(Environment.SystemDirectory, "kernel32.dll");
    FileVersion? version = FileVersion.Of(kernel32);
    Expect.That(version is { Build: > 0 }, "kernel32 has a version resource");
    Expect.That(FileVersion.Of(Path.Combine(Path.GetTempPath(), "no-such.dll")) is null,
                "a missing file has no version");
  }

  private static void ThePinnedBuildComesFromOffsetsH() {
    string fromOffsetsH = $"{Shared.EdgeVersionMajor}.{Shared.EdgeVersionMinor}." +
                          $"{Shared.EdgeVersionBuild}.{Shared.EdgeVersionRevision}";
    Expect.Equal(fromOffsetsH, EdgeBuild.Pinned.ToString(), "the pinned build is offsets.h's");
    Expect.Equal("msedge.dll", Shared.EdgeModule, "the module is msedge.dll");
  }

  private static void OnlyTheDllFeaturesDependOnTheBuild() {
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      bool isDllFeature = s_dllFeatures.Contains(feature);
      Expect.Equal(isDllFeature, EdgeBuild.DependsOnPinnedBuild(feature), $"{feature} depends");
    }
  }

  private static Features AllOn() {
    var features = new Features();
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      features.Set(feature, true);
    }
    return features;
  }

  private static void ThePinnedBuildBlocksNothing() {
    Expect.Equal(0, EdgeBuild.Blocked(AllOn(), s_pinned, s_pinned).Count, "nothing blocked");
    Expect.Equal(0, EdgeBuild.Unsupported(s_pinned, s_pinned).Count, "nothing unsupported");
  }

  private static void AnUpdateBlocksTheDllFeaturesOnly() {
    IReadOnlyList<Feature> blocked = EdgeBuild.Blocked(AllOn(), Version("155.0.4300.2"), s_pinned);
    Expect.That(blocked.SequenceEqual(s_dllFeatures), "an update blocks the DLL features");
  }

  private static void ADowngradeIsAMismatchToo() {
    IReadOnlyList<Feature> unsupported = EdgeBuild.Unsupported(Version("154.0.4100.1"), s_pinned);
    Expect.That(unsupported.SequenceEqual(s_dllFeatures), "a downgrade unsupports them too");
  }

  private static void FeaturesAlreadyOffAreNotReported() {
    var onlyTheming = new Features { NativeSplit = false, Theming = true, QuietDevMode = true };
    IReadOnlyList<Feature> blocked =
        EdgeBuild.Blocked(onlyTheming, Version("156.0.1.0"), s_pinned);
    Expect.That(blocked.SequenceEqual([Feature.Theming]), "only the switch that was on");
  }

  private static void AMismatchedProcessDecidesTheRunningBuild() {
    FileVersion other = Version("156.0.1.0");
    Expect.That(EdgeBuild.Judged([], s_pinned) is null, "no Edge, no build");
    Expect.Equal(s_pinned, EdgeBuild.Judged([s_pinned, s_pinned], s_pinned), "all pinned");
    Expect.Equal(other, EdgeBuild.Judged([s_pinned, other], s_pinned), "one other decides");
    Expect.Equal(other, EdgeBuild.Judged([other], s_pinned), "only another");
  }
}
