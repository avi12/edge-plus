using Microsoft.Win32;

namespace EdgePlus;

// The pair of registry keys Windows reads a startup entry from: the command
// line under Run, and Task Manager's on/off flag under StartupApproved\Run.
internal readonly record struct StartupKeys(string Run, string Approved) {
  // The real per-user keys; tests point at a throwaway pair instead.
  public static StartupKeys User {
    get;
  } = new(
      @"Software\Microsoft\Windows\CurrentVersion\Run",
      @"Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run");
}

// Starting the panel when Windows starts, through the per-user Run key.
//
// The Run key itself is the one source of truth: whether the entry exists is
// read from it, never remembered elsewhere. Task Manager's "Startup apps"
// switch does not touch the Run value; it writes a flag under the sibling
// StartupApproved\Run key, so that flag is honoured too. The entry's name is
// the Tauri panel's, so the two panels share one entry. The Tauri panel's
// startup.rs and edge-native startup.rs.
internal static class StartupEntry {
  // As Task Manager's "Startup apps" lists it.
  public const string Name = "Edge+";
  // Bit 0 of the first byte of a StartupApproved value is set when Task
  // Manager has turned the entry off (0x02 / 0x06 on, 0x03 / 0x07 off).
  private const byte ApprovedDisabledBit = 1;

  // What Windows runs at sign-in: the quoted panel path, then the switch that
  // starts it minimized.
  public static string RunCommand(string executable) => $"\"{executable}\" {Switches.Background}";

  public static bool IsDisabledFlag(byte[]? approved) =>
      approved is { Length: > 0 } && (approved[0] & ApprovedDisabledBit) != 0;

  // Whether Windows will run the entry at sign-in: the Run value exists and
  // Task Manager has not switched it off.
  public static bool IsEnabled(StartupKeys keys, string name) {
    using RegistryKey? run = Registry.CurrentUser.OpenSubKey(keys.Run);
    if (run?.GetValue(name) is null) {
      return false;
    }
    using RegistryKey? approved = Registry.CurrentUser.OpenSubKey(keys.Approved);
    return !IsDisabledFlag(approved?.GetValue(name) as byte[]);
  }

  // Make Windows run `command` at sign-in, replacing an earlier entry and
  // clearing any Task Manager "disabled" flag so the switch really means on.
  public static void Enable(StartupKeys keys, string name, string command) {
    using (RegistryKey run = Registry.CurrentUser.CreateSubKey(keys.Run)) {
      run.SetValue(name, command);
    }
    DeleteValue(keys.Approved, name);
  }

  public static void Disable(StartupKeys keys, string name) {
    DeleteValue(keys.Run, name);
    DeleteValue(keys.Approved, name);
  }

  // A missing key or value is already the state wanted.
  private static void DeleteValue(string subkey, string name) {
    using RegistryKey? key = Registry.CurrentUser.OpenSubKey(subkey, writable: true);
    key?.DeleteValue(name, throwOnMissingValue: false);
  }
}
