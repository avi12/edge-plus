using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace EdgePlus;

// What happened to a local theme that changed on disk: Error says why the edit
// was not applied, null when it was.
internal sealed record ThemeRefresh(string Id, string? Error);

// Watching local themes for edits.
//
// A theme added from a folder, .zip or .crx is polled for changes while theming
// is on. When one settles (the same new fingerprint on two polls in a row, so a
// save that writes several files is taken once) the theme is re-checked and
// re-installed (ThemeLibrary.RefreshLocal); a broken edit is reported and the
// theme Edge is showing is left alone. The Tauri panel's watch.rs.
internal sealed class ThemeWatch {
  private static readonly TimeSpan s_pollInterval = TimeSpan.FromSeconds(1);

  private readonly Dictionary<string, int> _settled = [];
  private readonly Dictionary<string, int> _pending = [];

  // Raised on the watch thread.
  public event Action<ThemeRefresh>? Refreshed;

  private readonly PollThread _thread = new("Theme watch", s_pollInterval);

  public void Start() => _thread.Start(Poll, Poll);

  public void Stop() => _thread.Stop();

  private void Poll() {
    Settings settings = Settings.Load();
    bool isWatching = settings.Features.Theming;
    IEnumerable<ThemeEntry> localThemes =
        settings.Theming.Themes.Where(theme => theme.Source == ThemeSource.Local);
    foreach (ThemeEntry theme in localThemes) {
      int? current = Fingerprint(theme.Origin);
      if (current is not int fingerprint) {
        continue;
      }
      if (!_settled.TryGetValue(theme.Id, out int known)) {
        _settled[theme.Id] = fingerprint;
        continue;
      }
      bool isUnchanged = fingerprint == known;
      if (isUnchanged || !isWatching) {
        _pending.Remove(theme.Id);
        continue;
      }
      bool isSettled = _pending.TryGetValue(theme.Id, out int pending) && pending == fingerprint;
      if (!isSettled) {
        _pending[theme.Id] = fingerprint;
        continue;
      }
      _pending.Remove(theme.Id);
      _settled[theme.Id] = fingerprint;
      Refreshed?.Invoke(new ThemeRefresh(theme.Id, Refresh(theme)));
    }
  }

  private static string? Refresh(ThemeEntry theme) {
    try {
      ThemeLibrary.RefreshLocal(theme.Id, theme.Origin);
      return null;
    } catch (ThemeException error) {
      return error.Message;
    } catch (IOException error) {
      return error.Message;
    } catch (UnauthorizedAccessException error) {
      return error.Message;
    }
  }

  // A hash of every file's path, size and modified time under `path`, or of
  // the file itself; null when it is gone (a moved folder is not an edit).
  private static int? Fingerprint(string path) {
    try {
      var hash = new HashCode();
      if (File.Exists(path)) {
        AddFile(ref hash, new FileInfo(path));
        return hash.ToHashCode();
      }
      if (!Directory.Exists(path)) {
        return null;
      }
      IEnumerable<string> files = Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories)
                                      .Order(StringComparer.Ordinal);
      foreach (string file in files) {
        hash.Add(file, StringComparer.Ordinal);
        AddFile(ref hash, new FileInfo(file));
      }
      return hash.ToHashCode();
    } catch (IOException) {
      return null;
    } catch (UnauthorizedAccessException) {
      return null;
    }
  }

  private static void AddFile(ref HashCode hash, FileInfo file) {
    hash.Add(file.Length);
    hash.Add(file.LastWriteTimeUtc.Ticks);
  }
}
