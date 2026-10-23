using System;
using System.IO;
using System.Linq;
using Microsoft.Win32;

namespace EdgePlus;

// Approved holds Task Manager's on/off flag; tests point at a throwaway pair.
internal readonly record struct StartupKeys(string Run, string Approved) {
  public static StartupKeys User {
    get;
  } = new(
      @"Software\Microsoft\Windows\CurrentVersion\Run",
      @"Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run");
}

// The Run key is the one source of truth, never remembered elsewhere. Task
// Manager's "Startup apps" switch leaves the Run value alone and writes a flag
// under StartupApproved\Run, so that flag is honoured too.
internal static class StartupEntry {
  // The retired Tauri panel's name too, so MigrateFromTauri can take its entry over.
  public const string Name = "Edge+";
  // Set in the first byte when Task Manager turned the entry off (0x02 / 0x06
  // on, 0x03 / 0x07 off).
  private const byte ApprovedDisabledBit = 1;
  // From source and installed.
  private static readonly string[] s_tauriLaunchers = ["fix-edge-panel.exe", "Edge+.exe"];

  public static string RunCommand(string executable) => $"\"{executable}\" {Switches.Background}";

  public static bool IsDisabledFlag(byte[]? approved) =>
      approved is { Length: > 0 } && (approved[0] & ApprovedDisabledBit) != 0;

  public static bool IsEnabled(StartupKeys keys, string name) {
    using RegistryKey? run = Registry.CurrentUser.OpenSubKey(keys.Run);
    if (run?.GetValue(name) is null) {
      return false;
    }
    using RegistryKey? approved = Registry.CurrentUser.OpenSubKey(keys.Approved);
    return !IsDisabledFlag(approved?.GetValue(name) as byte[]);
  }

  // Clears any Task Manager "disabled" flag, so the switch really means on.
  public static void Enable(StartupKeys keys, string name, string command) {
    using (RegistryKey run = Registry.CurrentUser.CreateSubKey(keys.Run)) {
      run.SetValue(name, command);
    }
    DeleteValue(keys.Approved, name);
  }

  // Keeps Task Manager's on/off flag as the user set it.
  public static bool MigrateFromTauri(StartupKeys keys, string name, string executable) {
    using RegistryKey? run = Registry.CurrentUser.OpenSubKey(keys.Run, writable: true);
    if (run?.GetValue(name) is not string command) {
      return false;
    }
    if (!IsTauriCommand(command)) {
      return false;
    }
    run.SetValue(name, RunCommand(executable));
    return true;
  }

  public static bool IsTauriCommand(string command) {
    string launcher = Path.GetFileName(LauncherOf(command));
    return s_tauriLaunchers.Contains(launcher, StringComparer.OrdinalIgnoreCase);
  }

  private static string LauncherOf(string command) {
    string trimmed = command.Trim();
    if (trimmed.StartsWith('"')) {
      int closingQuote = trimmed.IndexOf('"', 1);
      return closingQuote < 0 ? trimmed[1..] : trimmed[1..closingQuote];
    }
    int space = trimmed.IndexOf(' ');
    return space < 0 ? trimmed : trimmed[..space];
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
