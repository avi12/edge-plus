import { defineMessaging } from "../../messaging/index.ts";
import { diff } from "../diff.ts";
import type { Json, StateProtocol } from "../protocol.ts";
import { isJsonObject, MessageType } from "../protocol.ts";
import { createStdioTransport, reserveStdout } from "../transports/stdio.ts";

// A host in a child process, for stdio.test.ts: it owns { count } and
// answers { kind: "increment" }, { kind: "fail" } and { kind: "log" } (which
// prints with console.log, to prove stdout stays the protocol's).

reserveStdout();
const messaging = defineMessaging<StateProtocol>(createStdioTransport(), {
  onError: error => process.stderr.write(`host: ${error.message}\n`)
});
let state: Json = { count: 0 };

// The window going away rejects what is waiting; the process ends with its
// stdin, so there is nothing to do about it.
function ignoreClosed(): void {}

function publish(next: Json): void {
  messaging.sendMessage(MessageType.patch, {
    name: "counter",
    ops: diff(state, next)
  }).catch(ignoreClosed);
  state = next;
}

messaging.onMessage(MessageType.action, message => {
  const action = message.data;
  const kind = isJsonObject(action) ? action["kind"] : undefined;
  const count = isJsonObject(state) ? state["count"] : undefined;
  const canIncrement = kind === "increment" && typeof count === "number";
  if (canIncrement) {
    publish({ count: count + 1 });
  } else if (kind === "fail") {
    throw new Error("asked to fail");
  } else if (kind === "log") {
    console.log("noise that must not reach stdout");
  }
});
messaging.sendMessage(MessageType.snapshot, {
  name: "counter",
  value: state
}).catch(ignoreClosed);
