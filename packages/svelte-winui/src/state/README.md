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
await channel.send<Action>({ kind: "toggle" }); // resolves on the host's reply, rejects with HostError
```

`State` and `Action` are the app's types (Edge+ generates them from Rust); the
channel checks that a value is JSON and trusts the host for its shape.

### Wire protocol

One JSON value per message, ordered, on any transport (`transports/`). On
stdio it is one line each, and stdout carries nothing else: call
`reserveStdout()` first so `console.log` goes to stderr.

| Direction | Message |
|---|---|
| host to window | `{type:"snapshot", name, value}` the whole value; a later one is merged in place |
| host to window | `{type:"patch", name, ops}` applied in place, in order |
| host to window | `{type:"reply", id, error}` `error` is `null` or text |
| window to host | `{type:"action", id, action}` |

Patch operations (paths are arrays of keys and indices; an empty path is the
root):

- `{op:"set", path, value}` sets a key or index (an index equal to the length
  appends)
- `{op:"remove", path}` deletes a key or removes an array element
- `{op:"splice", path, start, deleteCount, items}` changes a range of an array

A patch is applied property by property to the existing reactive object, so a
binding re-runs only when what it read changed. Send the patches of a change
before its reply: when `send` resolves, the state already shows the result.
`diff(previous, next)` makes the operations for a JavaScript host; a Rust host
sends the same shapes. A malformed message, a patch before its snapshot, a path
that does not exist or a reply for no action is reported to `onError` (stderr by
default) and dropped; the remedy is a new snapshot. Messages with other `type`s
(a host's own notices) go to `onOther`. When the link closes, waiting and later
`send`s reject with `HostClosedError`.

## Running

```
node --disable-warning=ExperimentalWarning --import ./examples/register-deps.ts --test "src/**/*.test.ts"
```

(from `packages/svelte-winui`; the examples borrow Edge+'s panel dependencies
until the package has its own). Type check: `tsc -p tsconfig.state.json`.
