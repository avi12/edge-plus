// What a generated component links its control to its props with. A prop
// becomes an effect that writes the control's property when it changes; an
// event prop becomes one subscription for the control's life; a bindable
// property also listens to the control and writes the user's change back.

type Unsubscribe = () => void;
type Subscribe = (handler: () => void) => Unsubscribe;

interface Access<Value> {
  read: () => Value;
  write: (value: Value & ({} | null)) => void;
}

export class Linker {
  private readonly cleanups: Unsubscribe[] = [];
  // True while this class writes a property, so the change event that write
  // raises is not taken for the user's.
  private isWriting = false;

  add(cleanup: Unsubscribe): void {
    this.cleanups.push(cleanup);
  }

  // Props given when the component is made get an effect each; a prop that
  // appears later is not followed (a spread that adds keys is not tracked).
  property<Props extends object, Key extends keyof Props>(
    props: Props,
    key: Key,
    apply: (value: Props[Key] & ({} | null)) => void
  ): void {
    const isGiven = key in props;
    if (isGiven) {
      $effect(() => {
        const value = props[key];
        if (value !== undefined) {
          this.write(() => apply(value));
        }
      });
    }
  }

  // The property the children are: a child, when there is one, is the value.
  contentProperty<Props extends { children?: unknown }, Key extends keyof Props>(
    props: Props,
    key: Key,
    apply: (value: Props[Key] & ({} | null)) => void
  ): void {
    const isGiven = key in props;
    if (isGiven) {
      $effect(() => {
        const value = props[key];
        const hasChildren = props.children !== undefined;
        const isWritable = value !== undefined && !hasChildren;
        if (isWritable) {
          this.write(() => apply(value));
        }
      });
    }
  }

  bindable<Bound extends object, Key extends keyof Bound>(
    bound: Bound,
    key: Key,
    access: Access<Bound[Key]>,
    subscribers: readonly Subscribe[]
  ): void {
    $effect(() => {
      const value = bound[key];
      if (value !== undefined) {
        this.write(() => access.write(value));
      }
    });
    for (const subscribe of subscribers) {
      this.add(subscribe(() => this.changedByUser(bound, key, access.read)));
    }
  }

  private changedByUser<Bound extends object, Key extends keyof Bound>(
    bound: Bound,
    key: Key,
    read: () => Bound[Key]
  ): void {
    if (this.isWriting) {
      return;
    }

    const current = read();
    const isChanged = current !== bound[key];
    if (isChanged) {
      bound[key] = current;
    }
  }

  private write(action: () => void): void {
    const wasWriting = this.isWriting;
    this.isWriting = true;
    try {
      action();
    } finally {
      this.isWriting = wasWriting;
    }
  }

  dispose(): void {
    for (const cleanup of this.cleanups.splice(0)) {
      cleanup();
    }
  }
}
