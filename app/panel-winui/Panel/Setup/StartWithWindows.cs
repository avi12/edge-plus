using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Windows.ApplicationModel;

namespace EdgePlus;

// The panel's one answer to "does Edge+ start with Windows", for both ways it
// ships. The plain exe writes the per-user Run key (StartupEntry). The Store
// package cannot: an uninstall would leave that entry behind pointing at a
// deleted exe, so it switches the startupTask its manifest declares instead,
// which Windows removes with the package and lists in Task Manager the same
// way.
internal static class StartWithWindows {
  // The manifest's desktop:StartupTask TaskId (Package.appxmanifest).
  private const string PackagedTaskId = "EdgePlusStartup";

  // Whether this process runs from an installed package: only a packaged
  // process has a Package.Current, and asking an unpackaged one throws.
  public static bool IsPackaged {
    get;
  } = ReadIsPackaged();

  public static bool IsEnabled() {
    if (!IsPackaged) {
      return StartupEntry.IsEnabled(StartupKeys.User, StartupEntry.Name);
    }
    StartupTaskState state = PackagedTask().State;
    return state is StartupTaskState.Enabled or StartupTaskState.EnabledByPolicy;
  }

  // In a dev build this points boot at the dev executable, which is what a
  // developer running the panel from source expects. A packaged task the user
  // switched off in Task Manager stays off: Windows lets only the user turn it
  // back on there, and IsEnabled then reads it as off.
  public static void Set(bool isOn) {
    if (IsPackaged) {
      SetPackaged(isOn);
      return;
    }
    if (isOn) {
      string executable = Environment.ProcessPath
          ?? throw new InvalidOperationException("The panel cannot name its own executable.");
      StartupEntry.Enable(StartupKeys.User, StartupEntry.Name, StartupEntry.RunCommand(executable));
    } else {
      StartupEntry.Disable(StartupKeys.User, StartupEntry.Name);
    }
  }

  private static void SetPackaged(bool isOn) {
    StartupTask task = PackagedTask();
    if (!isOn) {
      task.Disable();
      return;
    }
    StartupTaskState state = Task.Run(() => task.RequestEnableAsync().AsTask()).Result;
    if (state == StartupTaskState.DisabledByUser) {
      PanelLog.Write("start at sign-in is switched off in Task Manager; only the user can turn it on there");
    }
  }

  // Awaited off the calling thread: the window's UI thread is an STA, and a
  // WinRT call must not block a thread it may need to complete on.
  private static StartupTask PackagedTask() =>
      Task.Run(() => StartupTask.GetAsync(PackagedTaskId).AsTask()).Result;

  private static bool ReadIsPackaged() {
    try {
      _ = Package.Current.Id;
      return true;
    } catch (Exception error) when (error is InvalidOperationException or COMException) {
      return false;
    }
  }
}
