<script lang="ts">
  // A component shaped as the generated ones are, for element-ref.test.ts:
  // its rest props go to its control, and the attachments written on it are
  // spread onto its element.
  import { attachmentsOf, control } from "../host/control.ts";
  import type { NodeAttachment } from "../host/control.ts";
  import type { Snippet } from "svelte";

  interface Props {
    children?: Snippet;
    [attachment: symbol]: NodeAttachment;
  }

  const { children, ...rest }: Props = $props();
</script>

<fake-text-block {...attachmentsOf(rest)} {@attach control(() => {})}>{@render children?.()}</fake-text-block>
