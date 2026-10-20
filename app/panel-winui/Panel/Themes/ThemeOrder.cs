using System;
using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// Reordering the theme library: the order the user drags or keys themes into
// is the order of settings.json's themes array, and so the order of the list
// and of the light / dark pickers. Pure, so it is unit tested.
internal static class ThemeOrder {
  // `ids` with `id` moved `offset` places (negative is up), stopping at either
  // end; unchanged when `id` is not in it.
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

  // `themes` in the order `ids` gives. A theme `ids` does not name keeps its
  // place after the named ones (it was added while the user was dragging), and
  // an id with no theme is ignored (it was removed meanwhile).
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
