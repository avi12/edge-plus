import type { CoreMessage } from "../protocol.g.ts";
import type { Transport } from "svelte-winui/state";
import { MessageType } from "svelte-winui/state";

// The core speaks its own protocol (protocol.rs): a `view` message carries the
// whole View. The host channel's state is named, so the core's `view` is read
// as the snapshot of the state called "view"; every other message of the
// core's, and the actions and replies (which already have the channel's
// shape), pass through as they are.

export const viewStateName = "view";

// The core's own process, so its messages are trusted to be well formed.
function isCoreMessage(value: unknown): value is CoreMessage {
  return typeof value === "object" && value !== null && "type" in value;
}

// What the channel takes for itself: the View, and the reply that settles an
// action.
const channelTypes: ReadonlyArray<CoreMessage["type"]> = ["view", "reply"];

// The core's messages the window itself acts on.
export type OtherMessage = Exclude<CoreMessage, { type: "view" | "reply" }>;

export function isOtherMessage(value: unknown): value is OtherMessage {
  return isCoreMessage(value) && !channelTypes.includes(value.type);
}

export function viewAsSnapshot(transport: Transport): Transport {
  return {
    send(value) {
      transport.send(value);
    },
    listen: listener => transport.listen({
      ...listener,
      message(value) {
        const isView = isCoreMessage(value) && value.type === "view";
        if (isView) {
          listener.message({
            type: MessageType.snapshot,
            name: viewStateName,
            value: value.view
          });
        } else {
          listener.message(value);
        }
      }
    })
  };
}
