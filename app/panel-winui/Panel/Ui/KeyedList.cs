using System;
using System.Collections.Generic;

namespace EdgePlus;

// Brings a list in line with a wanted order of keys by touching only what
// changed: items whose key is gone are removed, missing keys are created, and
// items already there stay the same objects. A panel's add/delete transition
// then plays for exactly the rows that came or went, instead of for every row a
// clear-and-refill would recreate.
internal static class KeyedList {
  // `move(from, to)`, when given, moves an existing item in one step (an
  // ObservableCollection's Move, which a list animates as a reposition rather
  // than as a removal and an addition); without it a move is a remove then an
  // insert.
  public static void Sync<T>(IList<T> items, IReadOnlyList<string> wantedKeys,
                             Func<T, string> keyOf, Func<string, T> create,
                             Action<int, int>? move = null) {
    var wanted = new HashSet<string>(wantedKeys, StringComparer.Ordinal);
    for (int i = items.Count - 1; i >= 0; i--) {
      if (!wanted.Contains(keyOf(items[i]))) {
        items.RemoveAt(i);
      }
    }
    for (int i = 0; i < wantedKeys.Count; i++) {
      bool isInPlace = i < items.Count && keyOf(items[i]) == wantedKeys[i];
      if (isInPlace) {
        continue;
      }
      int existing = IndexOf(items, keyOf, wantedKeys[i], start: i + 1);
      if (existing >= 0 && move is not null) {
        move(existing, i);
        continue;
      }
      T item;
      if (existing >= 0) {
        item = items[existing];
        items.RemoveAt(existing);
      } else {
        item = create(wantedKeys[i]);
      }
      items.Insert(i, item);
    }
  }

  private static int IndexOf<T>(IList<T> items, Func<T, string> keyOf, string key, int start) {
    for (int i = start; i < items.Count; i++) {
      if (keyOf(items[i]) == key) {
        return i;
      }
    }
    return -1;
  }
}
