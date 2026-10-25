<script lang="ts">
  import { getContext } from "svelte";
  import type { CounterAction } from "./app-types.ts";
  import type { HostContext } from "./context.ts";
  import { hostContextKey } from "./context.ts";

  const { channel } = getContext<HostContext>(hostContextKey);
  let outcome = $state("no action yet");

  // An action's promise settles with the host's reply; a refusal rejects.
  async function run(action: CounterAction): Promise<void> {
    try {
      await channel.send(action);
      outcome = `${action.kind}: ok`;
    } catch (error) {
      outcome = `${action.kind}: ${error instanceof Error ? error.message : String(error)}`;
    }
  }
</script>

<stack-panel spacing={8}>
  <button onclick={() => run({ kind: "increment" })}>Host +1</button>
  <button onclick={() => run({ kind: "fail" })}>Host fails</button>
  <text-block>outcome: {outcome}</text-block>
</stack-panel>
