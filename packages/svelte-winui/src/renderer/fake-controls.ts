// fallow-ignore-file unused-class-member
// Stand-ins for WinUI's controls, for the tests: plain objects with the
// members the renderer reads (a panel's children, a toggle's isOn and its
// Toggled event), registered under the names a tag spells. They run the real
// renderer (host.ts) without WinUI; the renderer reaches their members by
// name, which no static check follows.

class FakeCollection {
  readonly items: object[] = [];

  get size(): number {
    return this.items.length;
  }

  indexOf(item: object): number {
    return this.items.indexOf(item);
  }

  insertAt(index: number, item: object): void {
    this.items.splice(index, 0, item);
  }

  removeAt(index: number): void {
    this.items.splice(index, 1);
  }

  append(item: object): void {
    this.items.push(item);
  }

  clear(): void {
    this.items.length = 0;
  }
}

export class FakePanel {
  readonly margin = null;
  readonly children = new FakeCollection();
  // What the host last handed the class handler.
  className = "";
}

export class FakeToggle {
  readonly margin = null;
  isOn = false;
  onContent = "";
  subscriptions = 0;

  onToggled(_handler: (isOn: boolean) => void): () => void {
    this.subscriptions++;
    return () => {
      this.subscriptions--;
    };
  }
}

export class FakeBorder {
  readonly margin = null;
  child: object | null = null;
}

export const fakeControls = {
  FakePanel,
  FakeToggle,
  FakeBorder
};
