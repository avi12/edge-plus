import { diff } from "../../src/state/diff.ts";
import type { Json } from "../../src/state/protocol.ts";
import { isJsonObject, MessageType } from "../../src/state/protocol.ts";
import { createMessagePortTransport } from "../../src/state/transports/message-port.ts";
import { startWindowsAppSdk } from "../bootstrap.ts";
import { viewName } from "./app-types.ts";
import { MessageChannel, Worker } from "node:worker_threads";

// The counter example's host: it owns the app's state (a count and a
// history), sends it as a snapshot and then patches, and answers the actions
// the window sends. Here the host is this process's main thread and the link
// a worker MessagePort; a Rust or Electron host would speak the same
// messages over stdio (createStdioTransport) instead.
//
//   node --disable-warning=ExperimentalWarning --import ../register-deps.ts launch.ts

const maximumHistory = 5;

startWindowsAppSdk();
process.stdout.write(`pid ${process.pid}\n`);

const channel = new MessageChannel();
const worker = new Worker(new URL("./window.ts", import.meta.url), {
  workerData: {
    port: channel.port2
  },
  transferList: [channel.port2]
});
const transport = createMessagePortTransport(channel.port1);
let state: Json = {
  count: 0,
  history: []
};

function publish(next: Json): void {
  transport.send({
    type: MessageType.patch,
    name: viewName,
    ops: diff(state, next)
  });
  state = next;
}

transport.listen({
  message(value) {
    const action = isJsonObject(value) ? value["action"] : undefined;
    const isAction = isJsonObject(value) && value["type"] === MessageType.action && isJsonObject(action);
    if (!isAction) {
      return;
    }

    let error: string | null = null;
    const count = isJsonObject(state) ? state["count"] : undefined;
    const history = isJsonObject(state) ? state["history"] : undefined;
    const isIncrement = action["kind"] === "increment" && typeof count === "number" && Array.isArray(history);
    if (isIncrement) {
      publish({
        count: count + 1,
        history: [...history, count + 1].slice(-maximumHistory)
      });
    } else {
      error = "the host does not do that";
    }

    transport.send({
      type: MessageType.reply,
      id: value["id"],
      error
    });
  },
  malformed: reason => process.stderr.write(`host: unreadable message: ${reason}\n`),
  closed: () => process.exit(0)
});
transport.send({
  type: MessageType.snapshot,
  name: viewName,
  value: state
});
worker.on("exit", code => process.exit(code));
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exit(1);
});
