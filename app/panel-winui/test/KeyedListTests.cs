using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

internal static class KeyedListTests {
  // Made tells a kept item from a recreated one.
  private sealed record Item(string Key, int Made);

  public static void Run() {
    int made = 0;
    Item Create(string key) => new(key, made++);
    var items = new List<Item>();

    KeyedList.Sync(items, ["a", "b"], item => item.Key, Create);
    Expect.That(Keys(items) == "a,b", "an empty list is filled in order");
    Item a = items[0];
    Item b = items[1];

    KeyedList.Sync(items, ["a", "x", "b"], item => item.Key, Create);
    Expect.That(Keys(items) == "a,x,b", "a new key is inserted where it belongs");
    Expect.That(ReferenceEquals(items[0], a) && ReferenceEquals(items[2], b),
                "the items around it are kept, not recreated");

    KeyedList.Sync(items, ["a", "b"], item => item.Key, Create);
    Expect.That(Keys(items) == "a,b", "a gone key is removed");
    Expect.That(ReferenceEquals(items[0], a) && ReferenceEquals(items[1], b), "the rest kept");

    KeyedList.Sync(items, ["b", "a"], item => item.Key, Create);
    Expect.That(Keys(items) == "b,a", "a reorder moves items");
    Expect.That(ReferenceEquals(items[0], b) && ReferenceEquals(items[1], a),
                "a moved item is the same item");

    int madeBefore = made;
    KeyedList.Sync(items, ["b", "a"], item => item.Key, Create);
    Expect.Equal(madeBefore, made, "an unchanged list makes nothing");

    var moves = new List<(int From, int To)>();
    void Move(int from, int to) {
      moves.Add((from, to));
      Item moving = items[from];
      items.RemoveAt(from);
      items.Insert(to, moving);
    }
    KeyedList.Sync(items, ["a", "b"], item => item.Key, Create, Move);
    Expect.That(Keys(items) == "a,b", "a move callback reorders too");
    Expect.That(moves.SequenceEqual([(1, 0)]), "as one move, from 1 to 0");
    madeBefore = made;
    KeyedList.Sync(items, ["c", "a", "b"], item => item.Key, Create, Move);
    Expect.That(Keys(items) == "c,a,b" && made == madeBefore + 1,
                "a new key is still created, not moved");

    KeyedList.Sync(items, [], item => item.Key, Create);
    Expect.Equal(0, items.Count, "everything gone empties the list");
  }

  private static string Keys(List<Item> items) => string.Join(',', items.Select(item => item.Key));
}
