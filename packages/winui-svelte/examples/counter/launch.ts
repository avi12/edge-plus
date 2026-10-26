import { startWindowsAppSdk } from "../bootstrap.ts";
import { viewName } from "./app-types.ts";
import { MessageChannel, Worker } from "node:worker_threads";
import type { Json } from "winui-svelte/state/host";
import { createMessagePortTransport, diff, isJsonObject, MessageType } from "winui-svelte/state/host";

// The counter example's host: it owns the app's state (a count and a
// history), sends it as a snapshot and then patches, and answers the actions
// the window sends. Here the host is this process's main thread and the link
// a worker MessagePort; a Rust or Electron host would speak the same
// messages over stdio (createStdioTransport) instead.
//
//   node --disable-warning=ExperimentalWarning examples/counter/launch.ts
//
// (from the package's folder: the Windows App SDK's bootstrapper is looked for
// in its .winapp)

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
const messaging = defineMessaging<StateProtocol>(createMessagePortTransport(channel.port1), {
  onError: error => process.stderr.write(
    `host: ${error.message}
`
  )
});
let state: Json = {
  count: 0,
  history: []
};

function publish(next: Json): void {
  void messaging.sendMessage(MessageType.patch, {
    name: viewName,
    ops: diff(state, next)
  });
  state = next;
}

// A refusal is a thrown error: the window's `send` rejects with its text.
messaging.onMessage(MessageType.action, message => {
  const action = message.data;
  const count = isJsonObject(state) ? state["count"] : undefined;
  const history = isJsonObject(state) ? state["history"] : undefined;
  const isIncrement = isJsonObject(action) && action["kind"] === "increment" && typeof count === "number" && Array.isArray(history);
  if (!isIncrement) {
    throw new Error("the host does not do that");
  }

  publish({
    count: count + 1,
    history: [...history, count + 1].slice(-maximumHistory)
  });
});
void messaging.sendMessage(MessageType.snapshot, {
  name: viewName,
  value: state
});
worker.on("exit", code => process.exit(code));
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exit(1);
});
