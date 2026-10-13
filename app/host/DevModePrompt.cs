using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using System.Threading;

namespace FixEdge;

// Hides Edge's "Turn off extensions in developer mode" prompt by snoozing it
// for good. Edge keeps the snooze in each profile's Preferences as
// extensions.ui.dev_mode_warning_snooze_end_time - a time in microseconds since
// 1601, written as a string like every time pref Edge stores, and not one of
// the MAC-protected prefs (docs/edge-split-view.md has the neighbouring
// findings).
//
// Edge rewrites Preferences from memory while it runs, so an edit only sticks
// when made after Edge has fully exited. Arm() therefore starts a detached
// copy of this exe that waits for that moment, writes the snooze into every
// profile and quits; the prompt is checked when Edge starts, which is exactly
// when the edit is waiting for it.
internal static class DevModePrompt {
  public const string AfterExitSwitch = "--quiet-dev-mode-after-exit";
  private const string UserDataSwitch = "--user-data-dir=";
  private const string MutexName = @"Local\fix-edge.dev-mode-prompt";
  private const string LockfileName = "lockfile";

  private static readonly string[] s_snoozeKeyPath =
      ["extensions", "ui", "dev_mode_warning_snooze_end_time"];
  private static readonly TimeSpan s_snoozeLength = TimeSpan.FromDays(365 * 100);
  private static readonly TimeSpan s_pollInterval = TimeSpan.FromSeconds(2);
  private static readonly DateTime s_windowsEpoch = new(1601, 1, 1, 0, 0, 0, DateTimeKind.Utc);

  private static readonly string s_defaultUserData = Path.Combine(
      Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
      "Microsoft", "Edge", "User Data");

  // Starts the waiting copy of the host outside Edge's job, so Edge closing
  // does not take it down. The panel calls this too, when the switch turns on.
  public static void Arm() {
    string commandLine = $"\"{Paths.HostExecutable}\" {AfterExitSwitch}";
    bool isStarted = StartDetached(commandLine, CreateBreakawayFromJob) ||
                     StartDetached(commandLine, 0);
    if (!isStarted) {
      Process.Start(new ProcessStartInfo(Paths.HostExecutable, AfterExitSwitch) {
        UseShellExecute = false,
        CreateNoWindow = true,
      });
    }
  }

  // One waiter at a time: every Edge session's host arms one, and they would
  // otherwise pile up while Edge stays open.
  public static void WaitForEdgeExitThenSnooze(string[] args) {
    using var waiter = new Mutex(true, MutexName, out bool isOnlyWaiter);
    if (!isOnlyWaiter) {
      return;
    }
    string userData = args.FirstOrDefault(arg => arg.StartsWith(UserDataSwitch, StringComparison.Ordinal))
        ?[UserDataSwitch.Length..] ?? s_defaultUserData;
    while (IsInUse(userData)) {
      Thread.Sleep(s_pollInterval);
    }
    if (!FeatureSettings.IsEnabled(Catalog.QuietDevMode)) {
      return;
    }
    SnoozeEveryProfile(userData);
  }

  // Edge holds User Data\lockfile open, unshared, for as long as its browser
  // process runs - the exact "this Edge is still up" test, unlike a process
  // name, which another Edge with its own user data dir would also match.
  private static bool IsInUse(string userData) {
    string lockfile = Path.Combine(userData, LockfileName);
    if (!File.Exists(lockfile)) {
      return false;
    }
    try {
      using var probe = new FileStream(lockfile, FileMode.Open, FileAccess.ReadWrite, FileShare.None);
      return false;
    } catch (IOException) {
      return true;
    }
  }

  private static void SnoozeEveryProfile(string userData) {
    string snoozeEnd = ((DateTime.UtcNow + s_snoozeLength - s_windowsEpoch).Ticks / 10).ToString();
    foreach (string profile in ProfileDirectories(userData)) {
      string preferences = Path.Combine(userData, profile, "Preferences");
      if (!File.Exists(preferences)) {
        continue;
      }
      if (JsonNode.Parse(File.ReadAllText(preferences)) is not JsonObject root) {
        continue;
      }
      JsonObject parent = root;
      foreach (string key in s_snoozeKeyPath[..^1]) {
        var child = parent[key] as JsonObject ?? new JsonObject();
        parent[key] = child;
        parent = child;
      }
      parent[s_snoozeKeyPath[^1]] = snoozeEnd;
      Paths.WriteAtomically(preferences, root.ToJsonString());
    }
  }

  // Local State lists every profile Edge knows, by folder name.
  private static string[] ProfileDirectories(string userData) {
    string localState = Path.Combine(userData, "Local State");
    if (!File.Exists(localState)) {
      return [];
    }
    var profiles = JsonNode.Parse(File.ReadAllText(localState))?["profile"]?["info_cache"] as JsonObject;
    return profiles?.Select(profile => profile.Key).ToArray() ?? [];
  }

  private const uint CreateBreakawayFromJob = 0x01000000;
  private const uint DetachedProcess = 0x00000008;
  private const uint CreateNoWindow = 0x08000000;

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  private struct StartupInfo {
    public int Size;
    public IntPtr Reserved, Desktop, Title;
    public int X, Y, XSize, YSize, XCountChars, YCountChars, FillAttribute, Flags;
    public short ShowWindow, Reserved2;
    public IntPtr Reserved3, StdInput, StdOutput, StdError;
  }

  [StructLayout(LayoutKind.Sequential)]
  private struct ProcessInformation {
    public IntPtr Process, Thread;
    public int ProcessId, ThreadId;
  }

  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  private static extern bool CreateProcessW(
      string? applicationName, string commandLine, IntPtr processAttributes, IntPtr threadAttributes,
      bool inheritHandles, uint creationFlags, IntPtr environment, string? currentDirectory,
      ref StartupInfo startupInfo, out ProcessInformation processInformation);

  [DllImport("kernel32.dll")]
  private static extern bool CloseHandle(IntPtr handle);

  // A job that forbids breakaway refuses the first attempt; the second runs
  // inside it, which still outlives the host if the job is not kill-on-close.
  private static bool StartDetached(string commandLine, uint extraFlags) {
    var startupInfo = new StartupInfo { Size = Marshal.SizeOf<StartupInfo>() };
    bool isStarted = CreateProcessW(null, commandLine, IntPtr.Zero, IntPtr.Zero, false,
        DetachedProcess | CreateNoWindow | extraFlags, IntPtr.Zero, null,
        ref startupInfo, out ProcessInformation information);
    if (isStarted) {
      CloseHandle(information.Process);
      CloseHandle(information.Thread);
    }
    return isStarted;
  }
}
