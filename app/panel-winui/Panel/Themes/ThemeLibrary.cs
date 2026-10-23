using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace EdgePlus;

// The message is worded for the user.
internal sealed class ThemeException(string message) : Exception(message);

// Each change is installed through Edge's own loader, persisted, and pushed to
// the running Edge, so switching needs no restart.
internal static class ThemeLibrary {
  private const string CrxExtension = ".crx";
  private const string IdentityChanged = "The theme's identity changed; add it again.";
  // Edge installs a re-installed local theme a moment later; the DLL is asked to
  // re-apply the active theme once that has landed.
  private static readonly TimeSpan[] s_reapplyDelays = [
    TimeSpan.FromMilliseconds(1500), TimeSpan.FromMilliseconds(2500),
    TimeSpan.FromMilliseconds(5000),
  ];

  public static void AddStore(string url) {
    ParsedStoreTheme parsed =
        StoreUrl.Parse(url) ?? throw new ThemeException(Wording.NotAStoreLink);
    ExternalExtensions.InstallStore(parsed.Id, parsed.Store);
    Change(theming => theming.AddOrReplace(new ThemeEntry {
      Id = parsed.Id,
      Name = parsed.Name ?? InstalledName(parsed.Id) ?? parsed.Id,
      Source = ThemeSource.Store,
      Origin = url.Trim(),
    }));
  }

  // A folder, .zip or .crx.
  public static void AddLocal(string path) {
    (string id, string version, byte[] crx) = LocalThemeCrx(path);
    string stored = StoredCrx(id);
    Directory.CreateDirectory(Paths.ThemesRoot);
    File.WriteAllBytes(stored, crx);
    ExternalExtensions.InstallLocal(id, stored, version);
    string stem = Path.GetFileNameWithoutExtension(Path.TrimEndingDirectorySeparator(path));
    Change(theming => theming.AddOrReplace(new ThemeEntry {
      Id = id,
      Name = stem.Length > 0 ? stem : id,
      Source = ThemeSource.Local,
      Origin = path,
    }));
  }

  public static void Remove(string id) {
    ExternalExtensions.Uninstall(id);
    Change(theming => theming.Remove(id));
  }

  public static void Reorder(IReadOnlyList<string> ids) =>
      Change(theming => theming.Reorder(ids));

  public static void Assign(ColorMode mode, string id) =>
      Change(theming => theming.Assign(mode, id));

  // A theme that fails its checks throws before anything is installed, so what
  // Edge has is left as it was.
  public static void RefreshLocal(string id, string origin) {
    (string packedId, string version, byte[] crx) = LocalThemeCrx(origin);
    if (packedId != id) {
      throw new ThemeException(IdentityChanged);
    }
    string stored = StoredCrx(id);
    File.WriteAllBytes(stored, crx);
    ExternalExtensions.InstallLocal(id, stored, version);
    lock (Settings.Gate) {
      DllSync.Apply(Settings.Load());
    }
    foreach (TimeSpan delay in s_reapplyDelays) {
      Thread.Sleep(delay);
      DllChannel.SignalConfigChanged();
    }
  }

  private static string StoredCrx(string id) => Path.Combine(Paths.ThemesRoot, id + CrxExtension);

  // InvalidDataException messages from ThemeManifest and Crx are written for the
  // user; an I/O failure is logged and said plainly.
  private static (string Id, string Version, byte[] Crx) LocalThemeCrx(string source) {
    try {
      bool isCrx = string.Equals(Path.GetExtension(source), CrxExtension,
                                 StringComparison.OrdinalIgnoreCase);
      if (isCrx) {
        byte[] bytes = File.ReadAllBytes(source);
        CrxInfo info = Crx.Read(bytes);
        return (info.Id, info.Version, bytes);
      }
      PackedTheme packed = ThemePacker.Pack(source, Paths.ThemeKeys);
      return (packed.Id, packed.Version, packed.Crx);
    } catch (InvalidDataException error) {
      throw new ThemeException(error.Message);
    } catch (IOException error) {
      PanelLog.Write($"reading the theme at {source}", error);
      throw new ThemeException(Wording.ThemeFilesUnreadable);
    } catch (UnauthorizedAccessException error) {
      PanelLog.Write($"reading the theme at {source}", error);
      throw new ThemeException(Wording.ThemeFilesUnreadable);
    }
  }

  private static void Change(Action<Theming> edit) {
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      edit(settings.Theming);
      settings.Save();
      DllSync.Apply(settings);
    }
  }

  // A store link without a slug names nothing itself.
  private static string? InstalledName(string id) =>
      InstalledThemes.Scan(EdgeUserData.Default).FirstOrDefault(theme => theme.Id == id)?.Name;

  // The panel's own themes are installed in Edge too; each is listed once.
  public static InstalledTheme[] EdgeThemesNotIn(Theming theming) =>
      InstalledThemes.Scan(EdgeUserData.Default)
          .Where(theme => !theming.Themes.Any(own => own.Id == theme.Id))
          .ToArray();
}
