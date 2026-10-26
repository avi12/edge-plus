import { defineMessaging } from "../messaging/index.ts";
import type { ProtocolWithReturn } from "../messaging/index.ts";
import type { Json, StateProtocol } from "./protocol.ts";
import { MessageType, PatchOperation } from "./protocol.ts";
import "./test-support/svelte-runes.ts";
import { MemoryEnd } from "./transports/memory.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

const { createHostChannel, HostClosedError, HostError, hostState } = await import("./channel.svelte.ts");
const { track } = await import("./test-support/track.svelte.ts");

// A type alias, not an interface: only an alias is assignable to Json.
type View = {
  title: string;
  cards: Array<{
    title: string;
    isOn: boolean;
  }>;
};

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

// The app's own message types, on top of the state protocol.
const NoticeType = {
  notice: "notice",
  place: "place"
} as const;

interface NoticeProtocol {
  [NoticeType.notice]: ProtocolWithReturn<{ text: string }, void>;
  [NoticeType.place]: ProtocolWithReturn<number, string>;
}

// A channel on one end of a memory link and the host's messaging on the other.
// `actions` lists what the host received; it answers each by what `answer`
// returns (resolving at once by default).
function setup(onError: (error: Error) => void = error => {
  throw error;
}, answer: (action: Json) => Promise<void> | void = () => {}) {
  const [windowEnd, hostEnd] = MemoryEnd.pair();
  const channel = createHostChannel<Json, NoticeProtocol>(windowEnd, { onError });
  const host = defineMessaging<StateProtocol & NoticeProtocol>(hostEnd);
  const actions: Json[] = [];
  host.onMessage(MessageType.action, message => {
    actions.push(message.data);
    return answer(message.data);
  });
  return {
    channel,
    host,
    hostEnd,
    actions
  };
}

function ignore(): void {}

async function settle(): Promise<void> {
  await new Promise(resolve => setTimeout(resolve, 5));
}

test("a snapshot makes the state ready and `ready` settles with it", async () => {
  const { channel, host } = setup();
  const view = hostState<View>(channel, "view");
  assert.equal(view.isReady, false);
  assert.equal(view.current, undefined);
  void host.sendMessage(MessageType.snapshot, {
    name: "view",
    value: initial
  });
  assert.deepEqual(await view.ready, initial);
  assert.equal(view.isReady, true);
});

test("a patch changes only the bindings that read the changed property", async () => {
  const { channel, host } = setup();
  const view = hostState<View>(channel, "view");
  void host.sendMessage(MessageType.snapshot, {
    name: "view",
    value: initial
  });
  await view.ready;
  const firstTitle = track(() => view.current?.cards[0]?.title);
  const secondTitle = track(() => view.current?.cards[1]?.title);
  const cardCount = track(() => view.current?.cards.length);

  void host.sendMessage(MessageType.patch, {
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

  void host.sendMessage(MessageType.patch, {
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
  void host.sendMessage(MessageType.snapshot, {
    name: "view",
    value: initial
  });
  await view.ready;
  const firstTitle = track(() => view.current?.cards[0]?.title);
  const title = track(() => view.current?.title);
  void host.sendMessage(MessageType.snapshot, {
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

test("send resolves on the host's answer, after the patches sent before it", async () => {
  const { channel, host, actions } = setup(undefined, () => {
    void host.sendMessage(MessageType.patch, {
      name: "view",
      ops: [{
        op: PatchOperation.set,
        path: ["cards", 0, "isOn"],
        value: true
      }]
    });
  });
  const view = hostState<View>(channel, "view");
  void host.sendMessage(MessageType.snapshot, {
    name: "view",
    value: initial
  });
  await view.ready;
  await channel.send({
    kind: "toggle",
    index: 0
  });
  assert.deepEqual(actions, [{
    kind: "toggle",
    index: 0
  }]);
  assert.equal(view.current?.cards[0]?.isOn, true, "the state was updated before the promise settled");
});

test("answers out of order settle the action they belong to", async () => {
  const waiting = new Map<string, PromiseWithResolvers<void>>();
  const { channel } = setup(undefined, action => {
    const answer = Promise.withResolvers<void>();
    waiting.set(String(action), answer);
    return answer.promise;
  });
  const order: string[] = [];
  const first = channel.send("first").then(() => order.push("first"));
  const second = channel.send("second").then(() => order.push("second"));
  const third = channel.send("third").catch(error => order.push(`third:${String(error)}`));
  await settle();
  waiting.get("third")?.reject(new Error("no"));
  waiting.get("first")?.resolve();
  waiting.get("second")?.resolve();
  await Promise.all([first, second, third]);
  assert.deepEqual(order, ["third:HostError: no", "first", "second"]);
});

test("a host failure rejects with HostError carrying the host's text", async () => {
  const { channel } = setup(undefined, () => {
    throw new Error("Edge is not running");
  });
  await assert.rejects(channel.send("go"), error => error instanceof HostError && error.message === "Edge is not running");
});

test("malformed lines and messages are reported, dropped, and the channel goes on", async () => {
  const errors: string[] = [];
  const { channel, host, hostEnd } = setup(error => errors.push(error.message));
  const view = hostState<View>(channel, "view");
  hostEnd.sendMalformed("Unexpected token");
  hostEnd.send({
    id: 90,
    type: MessageType.snapshot,
    data: {
      value: 1
    }
  });
  void host.sendMessage(MessageType.patch, {
    name: "view",
    ops: [{
      op: PatchOperation.set,
      path: [],
      value: 1
    }]
  }).catch(ignore);
  hostEnd.send({
    id: 91,
    type: "unknown"
  });
  void host.sendMessage(MessageType.snapshot, {
    name: "view",
    value: initial
  });
  await view.ready;
  void host.sendMessage(MessageType.patch, {
    name: "view",
    ops: [{
      op: PatchOperation.set,
      path: ["nope", "deeper"],
      value: 1
    }]
  }).catch(ignore);
  void host.sendMessage(MessageType.patch, {
    name: "view",
    ops: [{
      op: PatchOperation.set,
      path: ["title"],
      value: "ok"
    }]
  });
  await settle();
  assert.equal(errors.length, 5, errors.join(" | "));
  assert.match(errors[0] ?? "", /Unexpected token/u);
  assert.match(errors[1] ?? "", /snapshot was malformed/u);
  assert.match(errors[2] ?? "", /before its snapshot/u);
  assert.match(errors[3] ?? "", /no handler for message "unknown"/u);
  assert.equal(view.current?.title, "ok", "later messages still apply");
});

test("the app's own message types are handlers on channel.messaging", async () => {
  const { channel, host } = setup(error => {
    throw error;
  });
  const notices: string[] = [];
  channel.messaging.onMessage(NoticeType.notice, message => {
    notices.push(message.data.text);
  });
  host.onMessage(NoticeType.place, message => `pane ${message.data}`);
  await host.sendMessage(NoticeType.notice, { text: "hi" });
  assert.deepEqual(notices, ["hi"]);
  assert.equal(await channel.messaging.sendMessage(NoticeType.place, 2), "pane 2");
});

test("closing the link rejects waiting actions and later sends", async () => {
  const { channel, hostEnd } = setup(undefined, () => new Promise<void>(() => {}));
  const waiting = channel.send("a");
  await settle();
  hostEnd.close();
  await assert.rejects(waiting, HostClosedError);
  await assert.rejects(channel.send("b"), HostClosedError);
});

test("two names are two independent states", async () => {
  const { channel, host } = setup();
  const left = hostState<Json>(channel, "left");
  const right = hostState<Json>(channel, "right");
  assert.equal(hostState<Json>(channel, "left"), left);
  void host.sendMessage(MessageType.snapshot, {
    name: "right",
    value: [1]
  });
  await right.ready;
  assert.equal(left.isReady, false);
  assert.deepEqual(right.current, [1]);
});
