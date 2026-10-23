using System;
using System.Collections.Generic;

namespace EdgePlus;

// Everything the window can ask for; the UI never touches settings, Edge or the
// DLL itself. Each can block (an injection waits for Edge), so the window calls
// them off the UI thread.
internal static class PanelActions {
  public static PanelState Load() => Read(Settings.Load());

  public static PanelState SetFeature(Feature feature, bool isOn) {
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      settings.Features.Set(feature, isOn);
      settings.Save();
      if (feature == Feature.QuietDevMode) {
        if (isOn) {
          ArmDevModeWaiter();
        }
      } else {
        DllSync.Apply(settings);
      }
      return Read(settings);
    }
  }

  public static PanelState SetStartWithWindows(bool isOn) {
    StartWithWindows.Set(isOn);
    return Load();
  }

  public static PanelState AddStoreTheme(string url) {
    ThemeLibrary.AddStore(url);
    return Load();
  }

  public static PanelState AddLocalTheme(string path) {
    ThemeLibrary.AddLocal(path);
    return Load();
  }

  public static PanelState RemoveTheme(string id) {
    ThemeLibrary.Remove(id);
    return Load();
  }

  public static PanelState ReorderThemes(IReadOnlyList<string> ids) {
    ThemeLibrary.Reorder(ids);
    return Load();
  }

  public static PanelState AssignTheme(ColorMode mode, string id) {
    ThemeLibrary.Assign(mode, id);
    return Load();
  }

  private static void ArmDevModeWaiter() {
    if (Environment.ProcessPath is string executable) {
      DetachedProcess.Start(DevModePrompt.WaiterCommandLine(executable));
    }
  }

  private static PanelState Read(Settings settings) {
    return new PanelState(
        Features: settings.Features,
        Theming: settings.Theming,
        EdgeThemes: ThemeLibrary.EdgeThemesNotIn(settings.Theming),
        IsEdgeRunning: EdgeProcesses.BrowserPids().Length > 0,
        IsStartWithWindows: StartWithWindows.IsEnabled(),
        IsSplitLoaded: DllChannel.IsLoaded(),
        EdgeBuild: EdgeWatch.CurrentBuild(),
        SupportedEdgeBuild: EdgeBuild.Pinned);
  }
}
