using System;
using System.Threading.Tasks;
using Windows.ApplicationModel;

namespace EdgePlus;

// The Store package uses its manifest's startup task, not the Run key, because
// an uninstall would leave a Run entry pointing at a deleted exe.
internal static class StartWithWindows {
  // Package.appxmanifest's desktop:StartupTask TaskId.
  private const string PackagedTaskId = "EdgePlusStartup";

  public static bool IsEnabled() {
    if (!Packaging.IsPackaged) {
      return StartupEntry.IsEnabled(StartupKeys.User, StartupEntry.Name);
    }
    StartupTaskState state = PackagedTask().State;
    return state is StartupTaskState.Enabled or StartupTaskState.EnabledByPolicy;
  }

  public static void Set(bool isOn) {
    if (Packaging.IsPackaged) {
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

  // Windows lets only the user re-enable a task switched off in Task Manager.
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

  // Blocking on the UI thread's STA could deadlock the WinRT call, so it runs
  // on the thread pool.
  private static StartupTask PackagedTask() =>
      Task.Run(() => StartupTask.GetAsync(PackagedTaskId).AsTask()).Result;
}
