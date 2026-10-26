import { read } from "../renderer/host/reflect.ts";
import { diff } from "./diff.ts";
import type { Holder } from "./patch.ts";
import { applyOps, PatchError, reconcile } from "./patch.ts";
import type { Json, PatchOp } from "./protocol.ts";
import { isJsonObject, patchDataSchema, PatchOperation, snapshotDataSchema } from "./protocol.ts";
import assert from "node:assert/strict";
import { test } from "node:test";
import type { z } from "zod";

function clone(value: Json): Json {
  return JSON.parse(JSON.stringify(value));
}

function valueAt(value: Json | undefined, ...path: Array<string | number>): Json | undefined {
  let current = value;
  for (const key of path) {
    if (Array.isArray(current)) {
      current = current[Number(key)];
    } else if (isJsonObject(current)) {
      current = current[String(key)];
    } else {
      return undefined;
    }
  }

  return current;
}

function holderOf(value: Json): Holder {
  return { current: clone(value) };
}

test("set changes one property and keeps its neighbours' identity", () => {
  const holder = holderOf({ cards: [{ title: "a" }, { title: "b" }] });
  const before = holder.current;
  const cardB = valueAt(holder.current, "cards", 1);
  applyOps(holder, [{
    op: PatchOperation.set,
    path: ["cards", 0, "title"],
    value: "A"
  }]);
  assert.equal(holder.current, before);
  assert.deepEqual(holder.current, { cards: [{ title: "A" }, { title: "b" }] });
  assert.equal(valueAt(holder.current, "cards", 1), cardB);
});

test("remove deletes a key or an array element; splice replaces a range", () => {
  const holder = holderOf({
    a: 1,
    b: 2,
    list: [1, 2, 3, 4]
  });
  applyOps(holder, [
    {
      op: PatchOperation.remove,
      path: ["a"]
    },
    {
      op: PatchOperation.remove,
      path: ["list", 0]
    },
    {
      op: PatchOperation.splice,
      path: ["list"],
      start: 1,
      deleteCount: 1,
      items: [9, 9]
    }
  ]);
  assert.deepEqual(holder.current, {
    b: 2,
    list: [2, 9, 9, 4]
  });
});

test("an empty path sets the root, merged in place", () => {
  const holder = holderOf({
    keep: {
      x: 1
    },
    drop: 1
  });
  const keep = valueAt(holder.current, "keep");
  applyOps(holder, [{
    op: PatchOperation.set,
    path: [],
    value: {
      keep: {
        x: 2
      }
    }
  }]);
  assert.deepEqual(holder.current, {
    keep: {
      x: 2
    }
  });
  assert.equal(valueAt(holder.current, "keep"), keep);
});

test("reconcile turns an array into the next one without replacing it", () => {
  const list = [{ n: 1 }, { n: 2 }, { n: 3 }];
  const first = list[0];
  assert.equal(reconcile(list, [{ n: 1 }, { n: 5 }]), list);
  assert.deepEqual(list, [{ n: 1 }, { n: 5 }]);
  assert.equal(list[0], first);
});

test("bad operations throw PatchError and never touch the prototype", () => {
  const holder = holderOf({
    list: [1],
    name: "x"
  });
  const bad: PatchOp[] = [
    {
      op: PatchOperation.set,
      path: ["missing", "deep"],
      value: 1
    },
    {
      op: PatchOperation.set,
      path: ["list", 5],
      value: 1
    },
    {
      op: PatchOperation.splice,
      path: ["name"],
      start: 0,
      deleteCount: 0,
      items: []
    },
    {
      op: PatchOperation.splice,
      path: ["list"],
      start: 9,
      deleteCount: 0,
      items: []
    },
    {
      op: PatchOperation.set,
      path: ["__proto__", "polluted"],
      value: true
    },
    {
      op: PatchOperation.remove,
      path: []
    }
  ];
  for (const operation of bad) {
    assert.throws(() => applyOps(holder, [operation]), PatchError, JSON.stringify(operation));
  }

  assert.equal(read({}, "polluted"), undefined);
});

test("diff then apply reproduces the next value, for many shapes", () => {
  const pairs: Array<[Json, Json]> = [
    [{ a: 1 }, { a: 2 }],
    [{ a: 1 }, { b: 1 }],
    [{ list: [1, 2, 3] }, { list: [1, 2, 3, 4] }],
    [{ list: [1, 2, 3] }, { list: [2, 3] }],
    [{ list: [1, 2, 3] }, { list: [1, 9, 3] }],
    [{ list: [{ t: "a" }, { t: "b" }] }, { list: [{ t: "b" }] }],
    [{ list: [{ t: "a" }, { t: "b" }] }, { list: [{ t: "a" }, { t: "B" }] }],
    [{ x: null }, {
      x: {
        y: [1]
      }
    }],
    [[1, 2], { a: 1 }],
    [1, 2]
  ];
  for (const [before, after] of pairs) {
    const holder = holderOf(before);
    applyOps(holder, diff(before, after));
    assert.deepEqual(holder.current, after, JSON.stringify([before, after]));
  }
});

test("an unchanged value diffs to nothing; a one-field change to one operation", () => {
  assert.deepEqual(diff({ a: [1, { b: 2 }] }, { a: [1, { b: 2 }] }), []);
  assert.equal(
    diff({
      a: {
        b: 1,
        c: 1
      }
    }, {
      a: {
        b: 2,
        c: 1
      }
    }).length, 1
  );
});

// What a message might hold: JSON, or a value JSON cannot carry.
type Candidate = Json | {
  name: string;
  value: undefined;
};

function accepts(schema: z.ZodType, value: Candidate): boolean {
  return schema.safeParse(value).success;
}

test("data schemas accept the protocol and reject the rest", () => {
  assert.ok(
    accepts(snapshotDataSchema, {
      name: "v",
      value: {
        a: [1]
      }
    })
  );
  assert.ok(
    accepts(patchDataSchema, {
      name: "v",
      ops: [{
        op: "remove",
        path: ["a", 0]
      }]
    })
  );
  assert.ok(!accepts(snapshotDataSchema, { value: 1 }));
  assert.ok(
    !accepts(patchDataSchema, {
      name: "v",
      ops: [{
        op: "move",
        path: []
      }]
    })
  );
  assert.ok(
    !accepts(patchDataSchema, {
      name: "v",
      ops: [{
        op: "splice",
        path: [],
        start: -1,
        deleteCount: 0,
        items: []
      }]
    })
  );
  assert.ok(
    !accepts(snapshotDataSchema, {
      name: "v",
      value: undefined
    })
  );
  assert.ok(!accepts(snapshotDataSchema, "snapshot"));
  assert.ok(!accepts(patchDataSchema, null));
});
