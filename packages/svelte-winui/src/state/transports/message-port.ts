import type { Transport, TransportListener } from "./transport.ts";
import type { MessagePort } from "node:worker_threads";
import { receiveMessageOnPort } from "node:worker_threads";

// A worker's MessagePort, as Edge+'s main thread and window thread use one.
//
// On a thread where Node's event loop does not turn (WinUI's loop holds it),
// the port's own "message" event never fires: pass `poll`, which calls the
// function it is given on a timer of the app's (a DispatcherQueue timer) and
// returns how to stop it. Without `poll` the port's events are used.

export interface MessagePortOptions {
  poll?(drain: () => void): () => void;
}

export function createMessagePortTransport(port: MessagePort, options: MessagePortOptions = {}): Transport {
  return {
    send(value) {
      port.postMessage(value);
    },
    listen(listener: TransportListener) {
      const { poll } = options;
      if (poll === undefined) {
        port.on("message", value => listener.message(value));
        port.on("close", () => listener.closed());
        return () => {
          port.removeAllListeners("message");
          port.removeAllListeners("close");
        };
      }

      const drain = (): void => {
        let received = receiveMessageOnPort(port);
        while (received !== undefined) {
          listener.message(received.message);
          received = receiveMessageOnPort(port);
        }
      };
      return poll(drain);
    }
  };
}
