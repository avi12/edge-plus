import { VIEW_STATE_NAME, viewAsSnapshot } from "./core-link.ts";
import { CoreMessageType } from "#src/protocol.g.ts";
import { strict as assert } from "node:assert";
import { describe, it } from "node:test";
import type { Json, Transport, TransportListener } from "winui-svelte/state/host";
import { MessageType } from "winui-svelte/state/host";

function recordingTransport() {
  const sent: object[] = [];
  let listener: TransportListener | undefined;
  const transport: Transport = {
    send(frame) {
      sent.push(frame);
    },
    listen(next) {
      listener = next;
      return () => {};
    }
  };
  return {
    transport,
    sent,
    deliver(value: Json) {
      listener?.message(value);
    }
  };
}

describe("the core's view request", () => {
  it("reaches the channel as the snapshot of the state named view, keeping its id", () => {
    const link = recordingTransport();
    const received: Json[] = [];
    viewAsSnapshot(link.transport).listen({
      message: value => received.push(value),
      malformed() {},
      closed() {}
    });
    link.deliver({
      id: 7,
      type: CoreMessageType.view,
      data: {
        productName: "Edge+"
      },
      timestamp: 5
    });
    assert.deepEqual(received, [
      {
        id: 7,
        type: MessageType.snapshot,
        data: {
          name: VIEW_STATE_NAME,
          value: {
            productName: "Edge+"
          }
        },
        timestamp: 5
      }
    ]);
  });

  it("lets every other frame through as it is, both ways", () => {
    const link = recordingTransport();
    const received: Json[] = [];
    const wrapped = viewAsSnapshot(link.transport);
    wrapped.listen({
      message: value => received.push(value),
      malformed() {},
      closed() {}
    });
    const notice = {
      id: 8,
      type: CoreMessageType.notice,
      data: "hello",
      timestamp: 6
    };
    link.deliver(notice);
    wrapped.send({
      id: 1,
      response: null
    });
    assert.deepEqual(received, [notice]);
    assert.deepEqual(link.sent, [
      {
        id: 1,
        response: null
      }
    ]);
  });
});
