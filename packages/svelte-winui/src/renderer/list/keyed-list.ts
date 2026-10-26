// Syncs a list to a wanted key order touching only what changed, so a panel's
// add/delete transition plays for exactly the rows that came or went, not for
// every row a clear-and-refill would recreate.

export interface KeyedItems<T> {
  readonly size: number;
  getAt(index: number): T;
  removeAt(index: number): void;
  insertAt(index: number, item: T): void;
}

export interface KeyedList<T> {
  items: KeyedItems<T>;
  keyOf(item: T): string;
  create(key: string): T;
  // A collection's own Move lets a list animate a move as a reposition rather
  // than as a removal and an addition.
  move?(from: number, to: number): void;
}

export function syncKeyed<T>(list: KeyedList<T>, wantedKeys: readonly string[]): void {
  removeUnwanted(list, new Set(wantedKeys));
  for (let i = 0; i < wantedKeys.length; i++) {
    place(list, wantedKeys[i], i);
  }
}

function removeUnwanted<T>(list: KeyedList<T>, wanted: Set<string>): void {
  for (let i = list.items.size - 1; i >= 0; i--) {
    if (!wanted.has(list.keyOf(list.items.getAt(i)))) {
      list.items.removeAt(i);
    }
  }
}

function keyAt<T>(list: KeyedList<T>, index: number): string | undefined {
  return index < list.items.size ? list.keyOf(list.items.getAt(index)) : undefined;
}

// Every key before `index` is already in place.
function place<T>(list: KeyedList<T>, key: string, index: number): void {
  if (keyAt(list, index) === key) {
    return;
  }

  const existing = indexOf(list, key, index + 1);
  if (existing < 0) {
    list.items.insertAt(index, list.create(key));
    return;
  }

  if (list.move !== undefined) {
    list.move(existing, index);
    return;
  }

  const item = list.items.getAt(existing);
  list.items.removeAt(existing);
  list.items.insertAt(index, item);
}

function indexOf<T>(list: KeyedList<T>, key: string, start: number): number {
  for (let i = start; i < list.items.size; i++) {
    if (list.keyOf(list.items.getAt(i)) === key) {
      return i;
    }
  }

  return -1;
}
