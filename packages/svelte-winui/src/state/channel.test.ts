import type { Json } from "./protocol.ts";
import { MessageType, PatchOperation } from "./protocol.ts";
import "./test-support/svelte-runes.ts";
import { MemoryEnd } from "./transports/memory.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

const { createHostChannel, HostClosedError, HostError, hostState } = await import("./channel.svelte.ts");
const { track } = await import("./test-support/track.svelte.ts");

interface View {
  title: string;
  cards: Array<{
    title: string;
    isOn: boolean;
  }>;
}

const initial: View = {
  title: "Edge+",
  cards: [{
    title: "split",
    isOn: false
  }, {
    title: "theme",
    isOn: true
  }]
};

// A channel on one end of a memory link and the host's end to drive by hand.
function setup(onError: (error: Error) => void = error => {
  throw error;
}, onOther?: (message: Record<string, unknown>) => void) {
  const [windowEnd, host] = MemoryEnd.pair();
  const channel = createHostChannel(windowEnd, {
    onError,
    ...(onOther === undefined ? {} : { onOther })
  });
  const received: unknown[] = [];
  host.listen({
    message: value => received.push(value),
    malformed() {},
    closed() {}
  });
  return {
    channel,
    host,
    received
  };
}

async function settle(): Promise<void> {
  await new Promise(resolve => setTimeout(resolve, 5));
}

test("a snapshot makes the state ready and `ready` settles with it", async () => {
  const { channel, host } = setup();
  const view = hostState<View>(channel, "view");
  assert.equal(view.isReady, false);
  assert.equal(view.current, undefined);
  host.send({
    type: MessageType.snapshot,
    name: "view",
    value: initial
  });
  assert.deepEqual(await view.ready, initial);
  assert.equal(view.isReady, true);
});

test("a patch changes only the bindings that read the changed property", async () => {
  const { channel, host } = setup();
  const view = hostState<View>(channel, "view");
  host.send({
    type: MessageType.snapshot,
    name: "view",
    value: initial
  });
  await view.ready;
  const firstTitle = track(() => view.current?.cards[0]?.title);
  const secondTitle = track(() => view.current?.cards[1]?.title);
  const cardCount = track(() => view.current?.cards.length);

  host.send({
    type: MessageType.patch,
    name: "view",
    ops: [{
      op: PatchOperation.set,
      path: ["cards", 1, "title"],
      value: "themes"
    }]
  });
  await settle();
  const { flushSync } = await import("svelte");
  flushSync();
  assert.deepEqual(firstTitle.values, ["split"], "the neighbour did not re-run");
  assert.deepEqual(secondTitle.values, ["theme", "themes"]);
  assert.deepEqual(cardCount.values, [2]);

  host.send({
    type: MessageType.patch,
    name: "view",
    ops: [{
      op: PatchOperation.splice,
      path: ["cards"],
      start: 2,
      deleteCount: 0,
      items: [{
        title: "x",
        isOn: false
      }]
    }]
  });
  await settle();
  flushSync();
  assert.deepEqual(cardCount.values, [2, 3]);
  assert.deepEqual(firstTitle.values, ["split"]);
  for (const tracked of [firstTitle, secondTitle, cardCount]) {
    tracked.stop();
  }
});

test("a later snapshot is merged in place: unchanged parts do not re-run", async () => {
  const { channel, host } = setup();
  const view = hostState<View>(channel, "view");
  host.send({
    type: MessageType.snapshot,
    name: "view",
    value: initial
  });
  await view.ready;
  const firstTitle = track(() => view.current?.cards[0]?.title);
  const title = track(() => view.current?.title);
  host.send({
    type: MessageType.snapshot,
    name: "view",
    value: {
      ...initial,
      title: "Edge Plus"
    }
  });
  await settle();
  const { flushSync } = await import("svelte");
  flushSync();
  assert.deepEqual(firstTitle.values, ["split"]);
  assert.deepEqual(title.values, ["Edge+", "Edge Plus"]);
  firstTitle.stop();
  title.stop();
});

test("send resolves on its reply, after the patches sent before the reply", async () => {
  const { channel, host, received } = setup();
  const view = hostState<View>(channel, "view");
  host.send({
    type: MessageType.snapshot,
    name: "view",
    value: initial
  });
  await view.ready;
  const done = channel.send({
    kind: "toggle",
    index: 0
  });
  await settle();
  assert.deepEqual(received, [{
    type: MessageType.action,
    id: 1,
    action: {
      kind: "toggle",
      index: 0
    }
  }]);
  host.send({
    type: MessageType.patch,
    name: "view",
    ops: [{
      op: PatchOperation.set,
      path: ["cards", 0, "isOn"],
      value: true
    }]
  });
  host.send({
    type: MessageType.reply,
    id: 1,
    error: null
  });
  await done;
  assert.equal(view.current?.cards[0]?.isOn, true, "the state was updated before the promise settled");
});

test("replies out of order settle the action they name", async () => {
  const { channel, host } = setup();
  const order: string[] = [];
  const first = channel.send("first").then(() => order.push("first"));
  const second = channel.send("second").then(() => order.push("second"));
  const third = channel.send("third").catch((error: unknown) => order.push(`third:${String(error)}`));
  await settle();
  host.send({
    type: MessageType.reply,
    id: 3,
    error: "no"
  });
  host.send({
    type: MessageType.reply,
    id: 1,
    error: null
  });
  host.send({
    type: MessageType.reply,
    id: 2,
    error: null
  });
  await Promise.all([first, second, third]);
  assert.deepEqual(order, ["third:HostError: no", "first", "second"]);
});

test("an error reply rejects with HostError carrying the host's text", async () => {
  const { channel, host } = setup();
  const failed = channel.send("go");
  host.send({
    type: MessageType.reply,
    id: 1,
    error: "Edge is not running"
  });
  await assert.rejects(failed, error => error instanceof HostError && error.message === "Edge is not running");
});

test("malformed lines and messages are reported, dropped, and the channel goes on", async () => {
  const errors: string[] = [];
  const { channel, host } = setup(error => errors.push(error.message));
  const view = hostState<View>(channel, "view");
  host.sendMalformed("Unexpected token");
  host.send({
    type: MessageType.snapshot,
    value: 1
  });
  host.send({
    type: MessageType.patch,
    name: "view",
    ops: [{
      op: PatchOperation.set,
      path: [],
      value: 1
    }]
  });
  host.send({
    type: MessageType.reply,
    id: 99,
    error: null
  });
  host.send({
    type: MessageType.snapshot,
    name: "view",
    value: initial
  });
  await view.ready;
  host.send({
    type: MessageType.patch,
    name: "view",
    ops: [{
      op: PatchOperation.set,
      path: ["nope", "deeper"],
      value: 1
    }]
  });
  host.send({
    type: MessageType.patch,
    name: "view",
    ops: [{
      op: PatchOperation.set,
      path: ["title"],
      value: "ok"
    }]
  });
  await settle();
  assert.equal(errors.length, 5, errors.join(" | "));
  assert.match(errors[0] ?? "", /unreadable message: Unexpected token/u);
  assert.match(errors[2] ?? "", /before its snapshot/u);
  assert.match(errors[3] ?? "", /action 99/u);
  assert.equal(view.current?.title, "ok", "later messages still apply");
});

test("the host's own message types go to onOther, not to the error handler", async () => {
  const others: unknown[] = [];
  const { host } = setup(error => {
    throw error;
  }, message => others.push(message));
  host.send({
    type: "notice",
    text: "hi"
  });
  await settle();
  assert.deepEqual(others, [{
    type: "notice",
    text: "hi"
  }]);
});

test("closing the link rejects waiting actions and later sends", async () => {
  const { channel, host } = setup();
  const waiting = channel.send("a");
  await settle();
  host.close();
  await assert.rejects(waiting, HostClosedError);
  await assert.rejects(channel.send("b"), HostClosedError);
});

test("two names are two independent states", async () => {
  const { channel, host } = setup();
  const left = hostState<Json>(channel, "left");
  const right = hostState<Json>(channel, "right");
  assert.equal(hostState<Json>(channel, "left"), left);
  host.send({
    type: MessageType.snapshot,
    name: "right",
    value: [1]
  });
  await right.ready;
  assert.equal(left.isReady, false);
  assert.deepEqual(right.current, [1]);
});
