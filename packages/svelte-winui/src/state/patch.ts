import type { Json, PatchOp, PathSegment } from "./protocol.ts";
import { isRecord, PatchOperation } from "./protocol.ts";

// Applies a host's changes to a value in place, so a reactive proxy (a
// `$state` object) only signals the properties that really changed: a
// binding reading `view.cards[2].title` re-runs when that title does, not
// when its neighbour does. Works on any plain object; it knows nothing of
// Svelte.

export class PatchError extends Error {}

// Names that would reach an object's prototype instead of its data.
const forbiddenKeys = new Set(["__proto__", "constructor", "prototype"]);

// What a state is kept in: the root can be replaced, so it sits in a holder.
export interface Holder {
  current: unknown;
}

function keyOf(segment: PathSegment): string {
  const key = String(segment);
  if (forbiddenKeys.has(key)) {
    throw new PatchError(`path segment "${key}" is not allowed`);
  }

  return key;
}

function indexIn(segment: PathSegment, limit: number): number {
  const isInRange = typeof segment === "number" && segment >= 0 && segment <= limit;
  if (!isInRange) {
    throw new PatchError(`array index ${String(segment)} is outside 0..${limit}`);
  }

  return segment;
}

function childOf(container: unknown, segment: PathSegment): unknown {
  if (Array.isArray(container)) {
    return container[indexIn(segment, container.length - 1)];
  }

  if (isRecord(container)) {
    const key = keyOf(segment);
    return Object.hasOwn(container, key) ? container[key] : undefined;
  }

  throw new PatchError(`cannot step into a ${typeof container}`);
}

function follow(root: unknown, path: PathSegment[]): unknown {
  let current = root;
  for (const segment of path) {
    current = childOf(current, segment);
    if (current === undefined) {
      throw new PatchError(`nothing at ${JSON.stringify(path)}`);
    }
  }

  return current;
}

// `next` merged into `previous`: equal parts keep their identity (and so
// their reactivity), differing parts are assigned. Returns what to store.
export function reconcile(previous: unknown, next: Json): unknown {
  if (Array.isArray(previous) && Array.isArray(next)) {
    for (let index = 0; index < next.length; index++) {
      previous[index] = reconcile(previous[index], next[index]);
    }

    previous.length = next.length;
    return previous;
  }

  if (isRecord(previous) && isRecord(next) && !Array.isArray(next)) {
    for (const key of Object.keys(previous)) {
      if (!Object.hasOwn(next, key)) {
        Reflect.deleteProperty(previous, key);
      }
    }

    for (const [key, value] of Object.entries(next)) {
      previous[keyOf(key)] = reconcile(Object.hasOwn(previous, key) ? previous[key] : undefined, value);
    }

    return previous;
  }

  return next;
}

function setAt(holder: Holder, path: PathSegment[], value: Json): void {
  const last = path.at(-1);
  if (last === undefined) {
    holder.current = reconcile(holder.current, value);
    return;
  }

  const parent = follow(holder.current, path.slice(0, -1));
  if (Array.isArray(parent)) {
    const index = indexIn(last, parent.length);
    parent[index] = reconcile(parent[index], value);
  } else if (isRecord(parent)) {
    const key = keyOf(last);
    parent[key] = reconcile(Object.hasOwn(parent, key) ? parent[key] : undefined, value);
  } else {
    throw new PatchError(`cannot set inside a ${typeof parent}`);
  }
}

function removeAt(holder: Holder, path: PathSegment[]): void {
  const last = path.at(-1);
  if (last === undefined) {
    throw new PatchError("the root cannot be removed");
  }

  const parent = follow(holder.current, path.slice(0, -1));
  if (Array.isArray(parent)) {
    parent.splice(indexIn(last, parent.length - 1), 1);
  } else if (isRecord(parent)) {
    Reflect.deleteProperty(parent, keyOf(last));
  } else {
    throw new PatchError(`cannot remove inside a ${typeof parent}`);
  }
}

function spliceAt(holder: Holder, op: Extract<PatchOp, { op: typeof PatchOperation.splice }>): void {
  const target = follow(holder.current, op.path);
  if (!Array.isArray(target)) {
    throw new PatchError(`splice needs an array at ${JSON.stringify(op.path)}`);
  }

  const isStartInRange = op.start <= target.length;
  if (!isStartInRange) {
    throw new PatchError(`splice start ${op.start} is past the end (${target.length})`);
  }

  target.splice(op.start, op.deleteCount, ...op.items);
}

// Operations apply in order. One that fails throws and leaves the earlier
// ones applied: the host answers by sending a snapshot.
export function applyOps(holder: Holder, ops: PatchOp[]): void {
  for (const op of ops) {
    if (op.op === PatchOperation.set) {
      setAt(holder, op.path, op.value);
    } else if (op.op === PatchOperation.remove) {
      removeAt(holder, op.path);
    } else {
      spliceAt(holder, op);
    }
  }
}
