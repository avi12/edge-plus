import type { HostChannel, HostState } from "../../src/state/index.ts";
import type { CounterView } from "./app-types.ts";

// What app.svelte shares with the components under it through Svelte's own
// context: the channel to send actions on and the host's state.
export interface HostContext {
  channel: HostChannel;
  view: HostState<CounterView>;
}

export const hostContextKey = Symbol("host");
