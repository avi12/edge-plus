using System;
using System.Collections.Generic;

namespace EdgePlus;

// Everything the window can ask for, each returning the state to render next.
// The Tauri panel's commands.rs: the UI never touches settings, Edge or the DLL
// itself. Each can block (an injection waits for Edge), so the window calls
// them off the UI thread.
internal static class PanelActions {
  public static PanelState Load() => Read(Settings.Load());

  public static PanelState SetFeature(Feature feature, bool isOn) {
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      settings.Features.Set(feature, isOn);
      settings.Save();
      // Hiding the developer-mode prompt is the panel's own: it arms the
      // after-exit waiter rather than anything in Edge.
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

  // In a dev build this points boot at the dev executable, which is what a
  // developer running the panel from source expects.
  public static PanelState SetStartWithWindows(bool isOn) {
    if (isOn) {
      string executable = Environment.ProcessPath
          ?? throw new InvalidOperationException("The panel cannot name its own executable.");
      StartupEntry.Enable(StartupKeys.User, StartupEntry.Name,
                          StartupEntry.RunCommand(executable));
    } else {
      StartupEntry.Disable(StartupKeys.User, StartupEntry.Name);
    }
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
    FileVersion? running = EdgeWatch.RunningBuild();
    IReadOnlyList<Feature> unsupported = running is FileVersion build
        ? EdgeBuild.Unsupported(build, EdgeBuild.Pinned)
        : [];
    return new PanelState(
        Features: settings.Features,
        Theming: settings.Theming,
        EdgeThemes: ThemeLibrary.EdgeThemesNotIn(settings.Theming),
        IsEdgeRunning: EdgeProcesses.BrowserPids().Length > 0,
        IsStartWithWindows: StartupEntry.IsEnabled(StartupKeys.User, StartupEntry.Name),
        IsSplitLoaded: DllChannel.IsLoaded(),
        EdgeBuild: running,
        SupportedEdgeBuild: EdgeBuild.Pinned,
        Unsupported: unsupported);
  }
}
