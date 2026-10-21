using System;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;

namespace EdgePlus;

// Snoozing Edge's "turn off extensions in developer mode" prompt.
//
// Edge keeps the snooze in each profile's Preferences as
// extensions.ui.dev_mode_warning_snooze_end_time (microseconds since 1601).
// Edge rewrites Preferences from memory while it runs, so the edit only sticks
// once Edge has fully exited — which is exactly when the prompt is next
// checked. The panel re-runs itself headless with a switch to do the waiting
// (Program), so the wait survives the panel window closing. A named mutex
// keeps it to one waiter at a time.
internal static class DevModePrompt {
  private const string WaiterMutex = @"Local\fix-edge.dev-mode-prompt";
  private const string LockfileName = "lockfile";
  private const string PreferencesFileName = "Preferences";
  private const string LocalStateFileName = "Local State";
  private const string TemporaryExtension = ".fixedge-tmp";
  private static readonly string[] s_snoozeKeyPath =
      ["extensions", "ui", "dev_mode_warning_snooze_end_time"];
  // A hundred years — "never ask again" in practice.
  private const long SnoozeMicroseconds = 100L * 365 * 24 * 60 * 60 * 1_000_000;
  private const long FileTimeTicksPerMicrosecond = 10;
  private static readonly TimeSpan s_pollInterval = TimeSpan.FromSeconds(2);

  // The command line that re-runs `executable` headless as the waiter; started
  // detached (DetachedProcess) so it outlives the panel window.
  public static string WaiterCommandLine(string executable) =>
      $"\"{executable}\" {Switches.DevModeWaiter}";

  // Wait until the Edge at `userData` has fully exited, then write the snooze
  // into every one of its profiles — unless the user switched the feature off
  // meanwhile. A no-op if another waiter already holds the mutex.
  public static void WaitForEdgeExitThenSnooze(string userData) {
    using var waiter = new Mutex(initiallyOwned: true, WaiterMutex, out bool isOnlyWaiter);
    if (!isOnlyWaiter) {
      return;
    }
    while (IsInUse(userData)) {
      Thread.Sleep(s_pollInterval);
    }
    if (!Settings.Load().Features.QuietDevMode) {
      return;
    }
    string snoozeEnd = SnoozeEnd(DateTime.UtcNow);
    foreach (string profile in ProfileFolders(userData)) {
      WriteSnooze(Path.Combine(userData, profile, PreferencesFileName), snoozeEnd);
    }
  }

  // When the snooze ends, in Edge's units: microseconds since 1601, as text.
  public static string SnoozeEnd(DateTime nowUtc) =>
      (nowUtc.ToFileTimeUtc() / FileTimeTicksPerMicrosecond + SnoozeMicroseconds).ToString(
          System.Globalization.CultureInfo.InvariantCulture);

  // Set the snooze in a parsed Preferences, creating the objects on its path
  // (and replacing any that are not objects).
  public static void SetSnooze(JsonObject preferences, string snoozeEnd) {
    JsonObject parent = preferences;
    foreach (string key in s_snoozeKeyPath[..^1]) {
      var child = parent[key] as JsonObject ?? new JsonObject();
      parent[key] = child;
      parent = child;
    }
    parent[s_snoozeKeyPath[^1]] = snoozeEnd;
  }

  // Edge holds User Data\lockfile open, unshared, for its whole run, so
  // failing to open it exclusively means Edge is still up.
  private static bool IsInUse(string userData) {
    string lockfile = Path.Combine(userData, LockfileName);
    if (!File.Exists(lockfile)) {
      return false;
    }
    try {
      using var probe = new FileStream(lockfile, FileMode.Open, FileAccess.Read, FileShare.None);
      return false;
    } catch (IOException) {
      return true;
    } catch (UnauthorizedAccessException) {
      return true;
    }
  }

  private static void WriteSnooze(string preferences, string snoozeEnd) {
    if (EdgeUserData.ReadJson(preferences) is not JsonObject root) {
      return;
    }
    SetSnooze(root, snoozeEnd);
    string temporary = Path.ChangeExtension(preferences, TemporaryExtension);
    try {
      File.WriteAllText(temporary, root.ToJsonString());
      File.Move(temporary, preferences, overwrite: true);
    } catch (IOException) {
    } catch (UnauthorizedAccessException) {
    }
  }

  // Local State lists every profile Edge knows, by folder name.
  private static string[] ProfileFolders(string userData) =>
      EdgeUserData.ReadJson(Path.Combine(userData, LocalStateFileName))?["profile"]?["info_cache"]
          is JsonObject profiles
          ? profiles.Select(profile => profile.Key).ToArray()
          : [];
}
