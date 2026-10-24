using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace EdgePlus;

// Before Edge+ had its name, its data lived in %LOCALAPPDATA%\FixEdge. An
// install from then carries over into the folder named for Edge+ the first time
// the panel starts. Once the new folder has settings it is never touched again,
// so a user of the renamed panel is never overwritten.
internal static class LegacyData {
  // The settings come last, so a copy cut short is tried again on the next start.
  public static bool Carry(string legacyRoot, string root, string settingsFileName,
                           IReadOnlyCollection<string> skippedFileNames) {
    bool isCarryNeeded =
        Directory.Exists(legacyRoot) && !File.Exists(Path.Combine(root, settingsFileName));
    if (!isCarryNeeded) {
      return false;
    }
    try {
      CopyInto(legacyRoot, root, skippedFileNames, settingsFileName);
      string legacySettings = Path.Combine(legacyRoot, settingsFileName);
      if (File.Exists(legacySettings)) {
        File.Copy(legacySettings, Path.Combine(root, settingsFileName));
      }
    } catch (IOException) {
      return false;
    } catch (UnauthorizedAccessException) {
      return false;
    }
    DeleteQuietly(legacyRoot);
    return true;
  }

  // `heldBackFileName` is only held back in the folder the carry started from.
  private static void CopyInto(string from, string to, IReadOnlyCollection<string> skippedFileNames,
                               string? heldBackFileName) {
    Directory.CreateDirectory(to);
    foreach (string file in Directory.EnumerateFiles(from)) {
      string name = Path.GetFileName(file);
      bool isSkipped = skippedFileNames.Contains(name, StringComparer.OrdinalIgnoreCase)
                       || string.Equals(name, heldBackFileName, StringComparison.OrdinalIgnoreCase);
      string target = Path.Combine(to, name);
      if (!isSkipped && !File.Exists(target)) {
        File.Copy(file, target);
      }
    }
    foreach (string folder in Directory.EnumerateDirectories(from)) {
      CopyInto(folder, Path.Combine(to, Path.GetFileName(folder)), skippedFileNames, null);
    }
  }

  // Held open by a DLL that has not unloaded yet: the copy is what counts.
  private static void DeleteQuietly(string folder) {
    try {
      Directory.Delete(folder, recursive: true);
    } catch (IOException) {
    } catch (UnauthorizedAccessException) {
    }
  }
}
