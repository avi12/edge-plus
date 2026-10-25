import type { Json, PatchOp, PathSegment } from "./protocol.ts";
import { isRecord, PatchOperation } from "./protocol.ts";

// For a host written in JavaScript (Node, Electron): the patch that turns one
// value into the next. A Rust host does the same with its own diff and sends
// the same operations.

function isSame(left: Json, right: Json): boolean {
  return JSON.stringify(left) === JSON.stringify(right);
}

function diffArrays(path: PathSegment[], previous: Json[], next: Json[], ops: PatchOp[]): void {
  let prefix = 0;
  const shortest = Math.min(previous.length, next.length);
  while (prefix < shortest && isSame(previous[prefix], next[prefix])) {
    prefix++;
  }

  let suffix = 0;
  while (suffix < shortest - prefix && isSame(previous[previous.length - 1 - suffix], next[next.length - 1 - suffix])) {
    suffix++;
  }

  const removed = previous.slice(prefix, previous.length - suffix);
  const inserted = next.slice(prefix, next.length - suffix);
  if (removed.length === inserted.length) {
    // Same length: the elements changed in place, so only they signal.
    inserted.forEach((item, offset) => diffValues([...path, prefix + offset], removed[offset], item, ops));
  } else {
    ops.push({
      op: PatchOperation.splice,
      path,
      start: prefix,
      deleteCount: removed.length,
      items: inserted
    });
  }
}

function diffValues(path: PathSegment[], previous: Json, next: Json, ops: PatchOp[]): void {
  if (isSame(previous, next)) {
    return;
  }

  if (Array.isArray(previous) && Array.isArray(next)) {
    diffArrays(path, previous, next, ops);
  } else if (isRecord(previous) && isRecord(next) && !Array.isArray(previous) && !Array.isArray(next)) {
    for (const key of Object.keys(previous)) {
      if (!Object.hasOwn(next, key)) {
        ops.push({
          op: PatchOperation.remove,
          path: [...path, key]
        });
      }
    }

    for (const [key, value] of Object.entries(next)) {
      const before = Object.hasOwn(previous, key) ? previous[key] : undefined;
      if (before === undefined) {
        ops.push({
          op: PatchOperation.set,
          path: [...path, key],
          value
        });
      } else {
        diffValues([...path, key], before, value, ops);
      }
    }
  } else {
    ops.push({
      op: PatchOperation.set,
      path,
      value: next
    });
  }
}

export function diff(previous: Json, next: Json): PatchOp[] {
  const ops: PatchOp[] = [];
  diffValues([], previous, next, ops);
  return ops;
}
