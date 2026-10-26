import type { Json } from "../../messaging/json.ts";
import { createMessagePortTransport } from "./message-port.ts";
import assert from "node:assert/strict";
import { test } from "node:test";
import { MessageChannel } from "node:worker_threads";

function listenerInto(seen: Json[]) {
  return {
    message: (value: Json) => seen.push(value),
    malformed() {},
    closed() {}
  };
}

test("messages cross a port by its events", async () => {
  const { port1, port2 } = new MessageChannel();
  const seen: Json[] = [];
  const stop = createMessagePortTransport(port1).listen(listenerInto(seen));
  createMessagePortTransport<Json>(port2).send({
    type: "x",
    n: 1
  });
  await new Promise(resolve => setTimeout(resolve, 20));
  stop();
  port1.close();
  port2.close();
  assert.deepEqual(seen, [{
    type: "x",
    n: 1
  }]);
});

test("with `poll`, nothing arrives until the app's timer drains the port", async () => {
  const { port1, port2 } = new MessageChannel();
  const seen: Json[] = [];
  const drainer = { drain(): void {} };
  createMessagePortTransport(port1, {
    poll(next) {
      drainer.drain = next;
      return () => {};
    }
  }).listen(listenerInto(seen));
  port2.postMessage({ n: 1 });
  port2.postMessage({ n: 2 });
  await new Promise(resolve => setTimeout(resolve, 20));
  assert.deepEqual(seen, []);
  drainer.drain();
  assert.deepEqual(seen, [{ n: 1 }, { n: 2 }]);
  port1.close();
  port2.close();
});
