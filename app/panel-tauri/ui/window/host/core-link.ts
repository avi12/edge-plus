import { CoreMessageType } from "#src/protocol.g.ts";
import type { Transport } from "winui-svelte/state/host";
import { MessageType } from "winui-svelte/state/host";
import { z } from "zod";

/** The name the core's `view` request is read as: the channel's state of that name. */
export const VIEW_STATE_NAME = "view";

const viewRequestSchema = z.object({
  id: z.number(),
  type: z.literal(CoreMessageType.view),
  data: z.json(),
  timestamp: z.number()
});

/**
 * The core sends its View as a `view` request; the channel's state protocol
 * calls that a snapshot. The frame keeps its id, so the channel's answer
 * settles the core's request. Every other frame goes through as it is.
 */
export function viewAsSnapshot(transport: Transport) {
  const wrapped: Transport = {
    send(value) {
      transport.send(value);
    },
    listen: listener => transport.listen({
      ...listener,
      message(value) {
        const request = viewRequestSchema.safeParse(value);
        if (!request.success) {
          listener.message(value);
          return;
        }

        listener.message({
          id: request.data.id,
          type: MessageType.snapshot,
          data: {
            name: VIEW_STATE_NAME,
            value: request.data.data
          },
          timestamp: request.data.timestamp
        });
      }
    })
  };
  return wrapped;
}
