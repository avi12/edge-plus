using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

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

  // Turning it on also injects into an Edge that is open without the DLL. Like
  // Inject, it returns once the DLL is live, so the state shows the outcome.
  public static PanelState SetAutoInject(bool isOn) {
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      settings.AutoInject = isOn;
      settings.Save();
      if (isOn) {
        DllSync.ApplyAndWaitForLoad(settings);
      }
      return Read(settings);
    }
  }

  // Turning it on looks for a newer release at once, so the card can appear
  // without waiting for the next scheduled check.
  public static PanelState SetAutoUpdate(bool isOn) {
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      settings.AutoUpdate = isOn;
      settings.Save();
      if (isOn) {
        BackgroundService.Current?.CheckForUpdateSoon();
      }
      return Read(settings);
    }
  }

  // Puts the downloaded release's exe in place of the running one and starts
  // it, which waits for this process to exit; the window then ends this
  // process. The new exe swaps its own DLL into Edge, so no Edge restart. A
  // copy the MSI installed runs the release's MSI once this process has exited
  // instead, and the MSI's update starts it again.
  public static void RestartToUpdate() {
    ReadyUpdate update = BackgroundService.Current?.PendingUpdate
        ?? throw new InvalidOperationException("No update is ready to install.");
    string running = Environment.ProcessPath
        ?? throw new InvalidOperationException("The panel cannot name its own executable.");
    if (update.IsInstaller) {
      RunInstallerAfterExit(update, running);
      return;
    }
    UpdateInstall.Install(update.File, running, update.Offer.Exe.Sha256, DateTime.UtcNow);
    Process.Start(new ProcessStartInfo(running) {
      UseShellExecute = false,
      ArgumentList = { Switches.RestartedFrom, Environment.ProcessId.ToString() },
    });
  }

  // Checked again here, since the download may have been altered since.
  private static void RunInstallerAfterExit(ReadyUpdate update, string running) {
    if (!UpdateInstall.IsDigestOf(update.File, update.Offer.Installer!.Sha256)) {
      throw new InvalidDataException("The downloaded installer does not match its digest.");
    }
    string arguments = MsiInstall.InstallerArguments(
        update.File,
        HasShortcut(Environment.SpecialFolder.CommonDesktopDirectory),
        HasShortcut(Environment.SpecialFolder.CommonPrograms));
    string script = MsiInstall.UpdateScript(Environment.ProcessId, arguments, running);
    bool isStarted = DetachedProcess.Start(MsiInstall.HiddenPowerShell(script));
    if (!isStarted) {
      throw new InvalidOperationException("The installer could not be started.");
    }
  }

  private static bool HasShortcut(Environment.SpecialFolder folder) =>
      File.Exists(MsiInstall.ShortcutFile(Environment.GetFolderPath(folder), Product.Name));

  // The Inject button: the DLL into every Edge browser process missing it. The
  // returned state's IsSplitLoaded is the outcome the button shows.
  public static PanelState Inject() {
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      DllSync.ApplyAndWaitForLoad(settings);
      return Read(settings);
    }
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
        IsSplitWanted: DllSync.IsWanted(settings),
        IsAutoInject: settings.AutoInject,
        IsAutoUpdate: settings.AutoUpdate,
        EdgeBuild: EdgeWatch.CurrentBuild(),
        SupportedEdgeBuild: EdgeBuild.Pinned);
  }
}
