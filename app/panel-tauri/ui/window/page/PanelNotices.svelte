<script lang="ts">
  import type { View } from "../../protocol.g.ts";
  import { usePanel } from "../host/panel.svelte.ts";
  import { pausedNotice } from "../wording/shared-wording.ts";
  import { InfoBar, TextBlock } from "winui-svelte";

  // What the page says above the cards: the hint to open Edge, the warning
  // that Edge's build is not the one Edge+ was made for, and the error of an
  // action that failed.
  const { view }: { view: View } = $props();
  const panel = usePanel();

  const buildNotice = $derived(pausedNotice(view.edge, view.paused));
</script>

<TextBlock class="card-description" margin="4,0,4,4" text="Open Edge to apply changes." visibility={view.isEdgeRunning ? "Collapsed" : "Visible"} />
<InfoBar isClosable={false} isOpen={buildNotice.length > 0} message={buildNotice} severity="Warning" />
<InfoBar
  isClosable={true}
  isOpen={panel.actionNotice.length > 0}
  message={panel.actionNotice}
  onClosed={() => {
    panel.actionNotice = "";
  }}
  severity="Error"
/>
