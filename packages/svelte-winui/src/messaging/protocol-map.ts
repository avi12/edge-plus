// Typed message passing in the shape of @webext-core/messaging: one
// ProtocolMap interface names every message, its data and what answering it
// returns, and both ends of a link are typed from it.
//
//   enum PingType { greet = "greet", count = "count" }
//   interface PingProtocol {
//     [PingType.greet]: ProtocolWithReturn<{ name: string }, string>;
//     [PingType.count]: ProtocolWithReturn<void, number>;
//   }
//
// The keys are an enum's members, so a message type is written once and
// never as a string literal at the producer and the consumer.

// A message whose data is TData and whose handler answers TReturn. The
// members only carry the types; they never exist at run time.
export interface ProtocolWithReturn<TData, TReturn> {
  readonly dataType?: TData;
  readonly returnType?: TReturn;
}

// A map may also be written with functions, as webext-core allows:
// `greet(data: { name: string }): string`.
export type GetDataType<TProtocol> = TProtocol extends (data: infer TData) => unknown ? TData
  : TProtocol extends ProtocolWithReturn<infer TData, unknown> ? TData
    : never;

export type GetReturnType<TProtocol> = TProtocol extends (data: never) => infer TReturn ? TReturn
  : TProtocol extends ProtocolWithReturn<unknown, infer TReturn> ? TReturn
    : never;

// What a handler is given: the data with the frame's identity.
export interface Message<TProtocolMap, TType extends keyof TProtocolMap> {
  id: number;
  type: TType;
  data: GetDataType<TProtocolMap[TType]>;
  timestamp: number;
}

export type MessageHandler<TProtocolMap, TType extends keyof TProtocolMap> = (
  message: Message<TProtocolMap, TType>
) => GetReturnType<TProtocolMap[TType]> | Promise<GetReturnType<TProtocolMap[TType]>>;

export type RemoveListener = () => void;

// `sendMessage("refresh")` for a message with no data, `sendMessage("greet",
// data)` for one with.
export type SendArguments<TProtocolMap, TType extends keyof TProtocolMap> =
  [GetDataType<TProtocolMap[TType]>] extends [void | undefined]
    ? [type: TType, data?: undefined]
    : [type: TType, data: GetDataType<TProtocolMap[TType]>];
