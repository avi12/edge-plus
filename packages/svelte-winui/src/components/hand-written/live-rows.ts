// The rows a virtualized list has made: one per realized container. A list
// shows a screenful of its items, so most keys have no row; the container of a
// row that scrolls away is recycled and takes the next row, and a row that is
// no longer shown must end (its effects, its controls), or a long list leaks
// one component per row it ever showed.

export interface LiveRow<Item> {
  // Shows `item` in the same row (the key stayed, what it stands for changed).
  update(item: Item): void;
  // Ends the row's component and lets go of its controls.
  unmount(): void;
}

interface Entry<Item, Container> {
  key: string;
  container: Container;
  row: LiveRow<Item>;
}

export class LiveRows<Item, Container> {
  private readonly entries = new Map<string, Entry<Item, Container>>();

  get count(): number {
    return this.entries.size;
  }

  // The container (by its id) now shows `key`. Its row stays when the key is
  // the same, else it is replaced by a new one from `create`.
  fill(containerId: string, container: Container, key: string, item: Item, create: () => LiveRow<Item>): void {
    const existing = this.entries.get(containerId);
    const isSameRow = existing !== undefined && existing.key === key;
    if (isSameRow) {
      existing.row.update(item);
      return;
    }

    existing?.row.unmount();
    this.entries.set(containerId, {
      key,
      container,
      row: create()
    });
  }

  // The container went to the recycle queue.
  release(containerId: string): void {
    this.entries.get(containerId)?.row.unmount();
    this.entries.delete(containerId);
  }

  // The items changed: every live row shows what its key stands for now. A key
  // that is gone keeps its row until its container is recycled.
  refresh(itemOf: (key: string) => Item | undefined): void {
    for (const entry of this.entries.values()) {
      const item = itemOf(entry.key);
      if (item !== undefined) {
        entry.row.update(item);
      }
    }
  }

  containers(): Container[] {
    return [...this.entries.values()].map(entry => entry.container);
  }

  clear(): void {
    for (const entry of this.entries.values()) {
      entry.row.unmount();
    }

    this.entries.clear();
  }
}
