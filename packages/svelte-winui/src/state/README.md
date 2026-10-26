# State

Svelte's own state tools work unchanged in a WinUI window; this folder adds
only what Svelte cannot know: state that lives in the host.

## Scheduling: nothing to do

App code never calls `flushSync`. WinUI's loop must be started with
`Application.startScheduled`; WinUI then calls back into JS with no script
under it, Node drains microtasks when each callback returns, and Svelte's
`queueMicrotask` flush runs. Proven for a control event, a dispatcher timer and
a host message by `examples/scheduling/run.ps1`. A host message is taken off its
port by a timer (WinUI holds the thread, so port events never fire):
`createMessagePortTransport(port, { poll })` takes that timer.

## Svelte's tools here

| Tool | Use | Exercised by |
|---|---|---|
| a class with `$state`/`$derived` in a `.svelte.ts` | window-only state shared by components | `examples/counter/tallies.svelte.ts`, `svelte-state.test.ts` |
| `SvelteMap` / `SvelteSet` | reactive collections, one signal per key | the same class (`buttonsUsed`), `svelte-state.test.ts` |
| context (`setContext`/`getContext`) | the channel and host state for a subtree | `examples/counter/App.svelte` and `HostButtons.svelte` |
| stores | a store from a library, read with `fromStore(store).current` | `svelte-state.test.ts` |

No store of our own is added: a `$state` object in a `.svelte.ts` is the store.

## Host state

State the host owns (Rust, Electron, Node) is one reactive object in the window.

```ts
const channel = createHostChannel(createStdioTransport());
const view = hostState<View>(channel, "view");   // View: the app's own type
view.current?.cards[0].title;                    // reactive, undefined before the snapshot
await view.ready;                                // the first snapshot has arrived
await channel.send({ kind: "toggle" });         // resolves when the host did it, rejects with HostError
```

`State` and the channel's `Action` (`createHostChannel<Action>`) are the app's
types (Edge+ generates them from Rust); the channel checks that a value is JSON
and trusts the host for its shape.

The channel is built on `messaging/` (typed messages after
@webext-core/messaging): `StateProtocol` is its ProtocolMap, keyed by
`MessageType`, and `channel.messaging` is the link itself. A host's own message
types are further handlers on it, with the app's protocol as the second type
parameter:

```ts
const channel = createHostChannel<Action, NoticeProtocol>(transport);
channel.messaging.onMessage(NoticeType.notice, message => show(message.data.text));
await channel.messaging.sendMessage(NoticeType.place, 2);
```

### Wire protocol

One JSON value per message, ordered, on any transport (`transports/`). On
stdio it is one line each, and stdout carries nothing else: call
`reserveStdout()` first so `console.log` goes to stderr.

Every message is a request frame and is answered by a response frame
(`messaging/`):

```
request   {id, type, data, timestamp}
response  {id, response}  or  {id, error}
```

| Direction | `type` | `data` | answered with |
|---|---|---|---|
| host to window | `snapshot` | `{name, value}` the whole value; a later one is merged in place | `null` |
| host to window | `patch` | `{name, ops}` applied in place, in order | `null` |
| window to host | `action` | the action | `null`, or `error` text when the host could not |

A host written in JavaScript uses `defineMessaging<StateProtocol>(transport)`
from `svelte-winui/messaging` and registers `onMessage(MessageType.action, ...)`
(a thrown error is the refusal); a Rust host reads and writes the frames.
Ids are the sender's own counter, and a response carries the id of its request.

Patch operations (paths are arrays of keys and indices; an empty path is the
root):

- `{op:"set", path, value}` sets a key or index (an index equal to the length
  appends)
- `{op:"remove", path}` deletes a key or removes an array element
- `{op:"splice", path, start, deleteCount, items}` changes a range of an array

A patch is applied property by property to the existing reactive object, so a
binding re-runs only when what it read changed. Send the patches of a change
before the action's answer: when `send` resolves, the state already shows the result.
`diff(previous, next)` makes the operations for a JavaScript host; a Rust host
sends the same shapes. A malformed message, a patch before its snapshot, a path
that does not exist or a `type` nobody handles is reported to `onError` (stderr
by default) and answered with an error; the remedy is a new snapshot. When the
link closes, waiting and later `send`s reject with `HostClosedError`.

## Running

```
node --disable-warning=ExperimentalWarning --import ./examples/register-deps.ts --test "src/**/*.test.ts"
```

(from `packages/svelte-winui`; the examples borrow Edge+'s panel dependencies
until the package has its own). Type check: `tsc -p tsconfig.state.json`.
