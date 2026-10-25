import type { KeyedItems, KeyedList } from "./keyed-list.ts";
import { syncKeyed } from "./keyed-list.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// `made` tells a kept item from a recreated one.
interface Item {
  key: string;
  made: number;
}

function arrayItems(items: Item[]): KeyedItems<Item> {
  return {
    get size() {
      return items.length;
    },
    getAt: index => items[index],
    removeAt(index) {
      items.splice(index, 1);
    },
    insertAt(index, item) {
      items.splice(index, 0, item);
    }
  };
}

function keys(items: Item[]): string {
  return items.map(item => item.key).join(",");
}

test("a keyed list is synced touching only what changed", () => {
  const items: Item[] = [];
  let made = 0;
  const moves: Array<[number, number]> = [];
  const list: KeyedList<Item> = {
    items: arrayItems(items),
    keyOf: item => item.key,
    create(key) {
      made++;
      return {
        key,
        made
      };
    }
  };

  syncKeyed(list, ["a", "b"]);
  assert.equal(keys(items), "a,b", "an empty list is filled in order");
  const [first, second] = items;

  syncKeyed(list, ["a", "x", "b"]);
  assert.equal(keys(items), "a,x,b", "a new key is inserted where it belongs");
  assert.ok(items[0] === first && items[2] === second, "the items around it are kept");

  syncKeyed(list, ["a", "b"]);
  assert.equal(keys(items), "a,b", "a gone key is removed");
  assert.ok(items[0] === first && items[1] === second, "the rest kept");

  syncKeyed(list, ["b", "a"]);
  assert.equal(keys(items), "b,a", "a reorder moves items");
  assert.ok(items[0] === second && items[1] === first, "a moved item is the same item");

  const madeBefore = made;
  syncKeyed(list, ["b", "a"]);
  assert.equal(made, madeBefore, "an unchanged list makes nothing");

  list.move = (from, to) => {
    moves.push([from, to]);
    const [moving] = items.splice(from, 1);
    items.splice(to, 0, moving);
  };
  syncKeyed(list, ["a", "b"]);
  assert.equal(keys(items), "a,b", "a move callback reorders too");
  assert.deepEqual(moves, [[1, 0]], "as one move, from 1 to 0");

  syncKeyed(list, ["c", "a", "b"]);
  assert.equal(keys(items), "c,a,b", "a new key is still created, not moved");
  assert.equal(made, madeBefore + 1);

  syncKeyed(list, []);
  assert.equal(items.length, 0, "everything gone empties the list");
});
