// Typed message passing over a transport, shaped like @webext-core/messaging.
// Nothing here uses runes or WinUI, so a Node host loads it as well as the
// window.

export type { RequestFrame, ResponseFrame } from "./frame.ts";
export { requestFrameSchema, responseFrameSchema } from "./frame.ts";
export type { Messaging, MessagingOptions } from "./messaging.ts";
export { defineMessaging, MessagingClosedError, MessagingError } from "./messaging.ts";
export type {
  GetDataType,
  GetReturnType,
  Message,
  MessageHandler,
  ProtocolWithReturn,
  RemoveListener,
  SendArguments
} from "./protocol-map.ts";
