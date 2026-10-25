import { diff } from "../diff.ts";
import type { Json } from "../protocol.ts";
import { isRecord, MessageType } from "../protocol.ts";
import { createStdioTransport, reserveStdout } from "../transports/stdio.ts";

// A host in a child process, for stdio.test.ts: it owns { count } and
// answers { kind: "increment" }, { kind: "fail" } and { kind: "log" } (which
// prints with console.log, to prove stdout stays the protocol's).

reserveStdout();
const transport = createStdioTransport();
let state: Json = { count: 0 };

function publish(next: Json): void {
  transport.send({
    type: MessageType.patch,
    name: "counter",
    ops: diff(state, next)
  });
  state = next;
}

transport.listen({
  message(value) {
    const action = isRecord(value) ? value["action"] : undefined;
    const isAction = isRecord(value) && value["type"] === MessageType.action && isRecord(action);
    if (!isAction) {
      return;
    }

    const kind = action["kind"];
    let error: string | null = null;
    if (kind === "increment" && isRecord(state) && typeof state["count"] === "number") {
      publish({ count: state["count"] + 1 });
    } else if (kind === "fail") {
      error = "asked to fail";
    } else if (kind === "log") {
      console.log("noise that must not reach stdout");
    }

    transport.send({
      type: MessageType.reply,
      id: value["id"],
      error
    });
  },
  malformed(reason) {
    process.stderr.write(`host could not read a line: ${reason}\n`);
  },
  closed() {
    process.exit(0);
  }
});
transport.send({
  type: MessageType.snapshot,
  name: "counter",
  value: state
});
