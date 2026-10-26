import { CoreMessageType } from "../../protocol.g.ts";
import type { Transport } from "winui-svelte/state";
import { MessageType } from "winui-svelte/state";
import { z } from "zod";

// The core speaks its own protocol (protocol.rs, as EdgePlusProtocolMap): its
// `view` request carries the whole View. The host channel's state is named, so
// the core's `view` is read as the snapshot of the state called "view" - the
// frame keeps its id, so the channel's answer settles the core's request.
// Every other request of the core's goes through as it is, to the handlers the
// session registers on the channel's messaging.

export const viewStateName = "view";

const viewRequestSchema = z.object({
  id: z.number(),
  type: z.literal(CoreMessageType.view),
  data: z.json(),
  timestamp: z.number()
});

export function viewAsSnapshot(transport: Transport): Transport {
  return {
    send(value) {
      transport.send(value);
    },
    listen: listener => transport.listen({
      ...listener,
      message(value) {
        const request = viewRequestSchema.safeParse(value);
        if (request.success) {
          listener.message({
            id: request.data.id,
            type: MessageType.snapshot,
            data: {
              name: viewStateName,
              value: request.data.data
            },
            timestamp: request.data.timestamp
          });
        } else {
          listener.message(value);
        }
      }
    })
  };
}
