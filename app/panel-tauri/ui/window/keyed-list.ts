// Syncs a list to a wanted key order touching only what changed, so a panel's
// add/delete transition plays for exactly the rows that came or went, not for
// every row a clear-and-refill would recreate.

export interface KeyedItems<T> {
  readonly size: number;
  getAt(index: number): T;
  removeAt(index: number): void;
  insertAt(index: number, item: T): void;
}

// `move` (a collection's own Move) lets a list animate a move as a reposition
// rather than as a removal and an addition.
export function syncKeyed<T>(
  items: KeyedItems<T>,
  wantedKeys: readonly string[],
  keyOf: (item: T) => string,
  create: (key: string) => T,
  move?: (from: number, to: number) => void
): void {
  const wanted = new Set(wantedKeys);
  for (let i = items.size - 1; i >= 0; i--) {
    if (!wanted.has(keyOf(items.getAt(i)))) {
      items.removeAt(i);
    }
  }
  for (let i = 0; i < wantedKeys.length; i++) {
    const wantedKey = wantedKeys[i];
    const isInPlace = i < items.size && keyOf(items.getAt(i)) === wantedKey;
    if (isInPlace) {
      continue;
    }
    const existing = indexOf(items, keyOf, wantedKey, i + 1);
    const canMove = existing >= 0 && move !== undefined;
    if (canMove) {
      move(existing, i);
      continue;
    }
    let item: T;
    if (existing >= 0) {
      item = items.getAt(existing);
      items.removeAt(existing);
    } else {
      item = create(wantedKey);
    }
    items.insertAt(i, item);
  }
}

function indexOf<T>(items: KeyedItems<T>, keyOf: (item: T) => string, key: string, start: number): number {
  for (let i = start; i < items.size; i++) {
    if (keyOf(items.getAt(i)) === key) {
      return i;
    }
  }
  return -1;
}
