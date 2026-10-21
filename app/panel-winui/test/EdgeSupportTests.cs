using System;
using System.IO;
using System.Linq;

namespace EdgePlus;

internal static class EdgeSupportTests {
  private static readonly FileVersion s_pinned = new(155, 0, 4283, 62);
  private static readonly FileVersion s_update = new(156, 0, 1, 0);

  public static void Run() {
    StatesFollowTheChoiceAndTheBuild();
    TheChoiceComesBackOnASupportedBuild();
    OnlyDllFeaturesPause();
    ToastOncePerBuild();
    TheInstalledBuildIsTheLaunchersVersion();
  }

  private static void StatesFollowTheChoiceAndTheBuild() {
    var choice = new Features { NativeSplit = true, Theming = false };
    Expect.Equal(FeatureState.On, EdgeSupport.StateOf(choice, Feature.NativeSplit, s_pinned, s_pinned),
                 "on, on the pinned build");
    Expect.Equal(FeatureState.Paused,
                 EdgeSupport.StateOf(choice, Feature.NativeSplit, s_update, s_pinned),
                 "on, on another build: paused");
    Expect.Equal(FeatureState.Unavailable,
                 EdgeSupport.StateOf(choice, Feature.Theming, s_update, s_pinned),
                 "off, on another build: unavailable");
    Expect.Equal(FeatureState.Off, EdgeSupport.StateOf(choice, Feature.Theming, s_pinned, s_pinned),
                 "off, on the pinned build");
    Expect.Equal(FeatureState.On, EdgeSupport.StateOf(choice, Feature.NativeSplit, null, s_pinned),
                 "an unknown build holds nothing back");
    Expect.That(EdgeSupport.IsSupported(s_pinned, s_pinned) && !EdgeSupport.IsSupported(
                    new FileVersion(154, 0, 4283, 62), s_pinned),
                "a downgrade is unsupported too");
  }

  private static void TheChoiceComesBackOnASupportedBuild() {
    var choice = new Features { NativeSplit = true, Theming = true, QuietDevMode = true };
    Features paused = EdgeSupport.Effective(choice, s_update, s_pinned);
    Expect.That(!paused.NativeSplit && !paused.Theming, "the DLL features apply as off");
    Expect.That(paused.QuietDevMode, "the panel's own feature keeps running");
    Expect.That(choice.NativeSplit && choice.Theming, "the user's choice itself is never rewritten");
    Features back = EdgeSupport.Effective(choice, s_pinned, s_pinned);
    Expect.That(back.NativeSplit && back.Theming, "a supported build brings the choice back");
  }

  private static void OnlyDllFeaturesPause() {
    var allOn = new Features();
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      allOn.Set(feature, true);
    }
    var paused = EdgeSupport.Paused(allOn, s_update, s_pinned);
    Expect.That(!paused.Contains(Feature.QuietDevMode), "quiet dev mode needs no DLL");
    Expect.Equal(Enum.GetValues<Feature>().Count(EdgeBuild.DependsOnPinnedBuild), paused.Count,
                 "every DLL feature pauses");
    Expect.Equal(0, EdgeSupport.Paused(allOn, s_pinned, s_pinned).Count, "none on the pinned build");
  }

  private static void ToastOncePerBuild() {
    Feature[] some = [Feature.NativeSplit];
    Expect.That(EdgeSupport.ShouldNotify(some, s_update, null), "first time for this build");
    Expect.That(!EdgeSupport.ShouldNotify(some, s_update, s_update), "not again for the same build");
    Expect.That(EdgeSupport.ShouldNotify(some, new FileVersion(156, 0, 2, 0), s_update),
                "again for the next build");
    Expect.That(!EdgeSupport.ShouldNotify([], s_update, null), "not when nothing is paused");
  }

  // The seam: a fake install folder holding any versioned msedge.exe.
  private static void TheInstalledBuildIsTheLaunchersVersion() {
    string folder = Path.Combine(Path.GetTempPath(), $"edgeplus-fake-edge-{Environment.ProcessId}");
    Directory.CreateDirectory(folder);
    string notepad = Path.Combine(Environment.SystemDirectory, "notepad.exe");
    File.Copy(notepad, Path.Combine(folder, EdgeInstall.LauncherName), overwrite: true);
    try {
      // Compared with the copy: Windows reports a System32 binary in place
      // through a version shim (6.2) that a copy outside it does not get.
      FileVersion? launcher = FileVersion.Of(Path.Combine(folder, EdgeInstall.LauncherName));
      Expect.That(launcher is { Build: > 0 }, "the fake launcher has a version");
      Expect.Equal(launcher, EdgeInstall.InstalledBuild(folder),
                   "the launcher's file version is the installed build");
      Expect.That(EdgeInstall.InstalledBuild(Path.Combine(folder, "nothing-here")) is null,
                  "no launcher, no build");
      Expect.That(EdgeInstall.InstalledBuild(null) is null, "no install, no build");
    } finally {
      Directory.Delete(folder, recursive: true);
    }
  }
}
