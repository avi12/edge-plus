import { syncKeyed } from "../../renderer/keyed-list.ts";

// The vector of keys a list control shows (a WinRT IVector of boxed strings),
// brought to a wanted order of keys.

export interface KeyVector<Boxed> {
  readonly size: number;
  getAt(index: number): Boxed;
  removeAt(index: number): void;
  insertAt(index: number, boxedKey: Boxed): void;
  replaceAll(boxedKeys: Boxed[]): void;
}

// How a key is held in the vector.
export interface KeyBox<Boxed> {
  box(key: string): Boxed;
  unbox(boxedKey: Boxed): string;
}

export function duplicateKey(keys: readonly string[]): string | undefined {
  return keys.find((key, index) => keys.indexOf(key) !== index);
}

// An empty vector is filled in one go (one reset for the list to lay out,
// rather than an insertion each); otherwise only the keys that came, went or
// moved are touched, so the list animates exactly those.
export function syncKeyVector<Boxed>(vector: KeyVector<Boxed>, codec: KeyBox<Boxed>, wanted: readonly string[]): void {
  const isEmpty = vector.size === 0;
  if (isEmpty) {
    vector.replaceAll(wanted.map(key => codec.box(key)));
    return;
  }

  syncKeyed({
    items: {
      get size() {
        return vector.size;
      },
      getAt: index => codec.unbox(vector.getAt(index)),
      removeAt: index => vector.removeAt(index),
      insertAt: (index, key) => vector.insertAt(index, codec.box(key))
    },
    keyOf: key => key,
    create: key => key
  }, wanted);
}
