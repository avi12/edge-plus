using System;
using System.IO;
using Microsoft.Windows.AppNotifications;
using Microsoft.Windows.AppNotifications.Builder;

namespace EdgePlus;

// The host registers the process for notifications and opens the window when
// the toast is clicked; this only builds and shows it.
internal static class PausedToast {
  // A simulation (EdgeInstall) remembers in memory only, so the real record
  // stays as it was.
  private static FileVersion? s_simulatedNotified;

  public static void ShowOncePerBuild(BuildChange change) {
    FileVersion? notifiedFor = NotifiedFor();
    if (!EdgeSupport.ShouldNotify(change.Paused, change.EdgeBuild, notifiedFor)) {
      return;
    }
    string body = Wording.DescribePaused(change.EdgeBuild, change.SupportedBuild, change.Paused);
    try {
      AppNotification toast = new AppNotificationBuilder()
                                  .AddText(Wording.PausedToastTitle)
                                  .AddText(body)
                                  .BuildNotification();
      AppNotificationManager.Default.Show(toast);
      Remember(change.EdgeBuild);
    } catch (Exception error) when (error is IOException or UnauthorizedAccessException
                                        or System.Runtime.InteropServices.COMException) {
      // No toast is no reason to stop listening; the card still says it.
      PanelLog.Write("paused toast failed", error);
    }
  }

  private static FileVersion? NotifiedFor() {
    if (EdgeInstall.IsSimulated) {
      return s_simulatedNotified;
    }
    try {
      return File.Exists(Paths.PausedToastBuild)
          ? FileVersion.Parse(File.ReadAllText(Paths.PausedToastBuild))
          : null;
    } catch (IOException) {
      return null;
    }
  }

  private static void Remember(FileVersion build) {
    if (EdgeInstall.IsSimulated) {
      s_simulatedNotified = build;
      return;
    }
    Paths.WriteAtomically(Paths.PausedToastBuild, build.ToString());
  }
}
