using System;
using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// The order of settings.json's themes array is the order of the list and of
// the light / dark pickers.
internal static class ThemeOrder {
  // Negative `offset` is up; stops at either end.
  public static IReadOnlyList<string> Moved(IReadOnlyList<string> ids, string id, int offset) {
    int from = ids.ToList().IndexOf(id);
    if (from < 0) {
      return ids;
    }
    int to = Math.Clamp(from + offset, 0, ids.Count - 1);
    var moved = ids.ToList();
    moved.RemoveAt(from);
    moved.Insert(to, id);
    return moved;
  }

  // A theme `ids` does not name goes after the named ones (it was added while
  // the user was dragging); an id with no theme was removed meanwhile.
  public static List<ThemeEntry> Ordered(IReadOnlyList<ThemeEntry> themes,
                                         IReadOnlyList<string> ids) {
    var byId = new Dictionary<string, ThemeEntry>(StringComparer.Ordinal);
    foreach (ThemeEntry theme in themes) {
      byId.TryAdd(theme.Id, theme);
    }
    var ordered = new List<ThemeEntry>();
    foreach (string id in ids) {
      if (byId.Remove(id, out ThemeEntry? theme)) {
        ordered.Add(theme);
      }
    }
    ordered.AddRange(themes.Where(theme => byId.ContainsKey(theme.Id)));
    return ordered;
  }
}
