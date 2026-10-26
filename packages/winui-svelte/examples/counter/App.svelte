<script lang="ts">
  import type { CounterView } from "./app-types.ts";
  import { viewName } from "./app-types.ts";
  import { hostContextKey } from "./context.ts";
  import HostButtons from "./HostButtons.svelte";
  import { Tallies } from "./tallies.svelte.ts";
  import { setContext } from "svelte";
  import type { HostChannel } from "winui-svelte/state";
  import { hostState } from "winui-svelte/state";

  interface Props {
    channel: HostChannel;
  }

  const { channel }: Props = $props();
  const view = hostState<CounterView>(channel, viewName);
  const tallies = new Tallies();
  setContext(hostContextKey, {
    channel,
    view
  });
</script>

<stack-panel padding="12,12,12,12" spacing={8}>
  <text-block>host count: {view.current?.count ?? "waiting"}</text-block>
  <text-block>host history length: {view.current?.history.length ?? 0}</text-block>
  <text-block>local count: {tallies.localCount} (doubled {tallies.doubled})</text-block>
  <text-block>buttons used: {[...tallies.buttonsUsed].join(",")}</text-block>
  <button onclick={() => tallies.bump("local")}>Local +1</button>
  <HostButtons />
</stack-panel>
