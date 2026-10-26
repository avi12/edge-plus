<script lang="ts">
  import type { View } from "../../protocol.g.ts";
  import { usePanel } from "../host/panel.svelte.ts";
  import { InfoBar, TextBlock } from "winui-svelte";

  // What the page says above the cards: the hint to open Edge, the warning
  // that Edge's build is not the one Edge+ was made for, and the error of an
  // action that failed.
  const { view }: { view: View } = $props();
  const panel = usePanel();
</script>

<TextBlock class="card-description" margin="4,0,4,4" text={view.openEdgeHint ?? ""} visibility={view.openEdgeHint === null ? "Collapsed" : "Visible"} />
<InfoBar isClosable={false} isOpen={view.buildNotice.length > 0} message={view.buildNotice} severity="Warning" />
<InfoBar
  isClosable={true}
  isOpen={panel.actionNotice.length > 0}
  message={panel.actionNotice}
  onClosed={() => {
    panel.actionNotice = "";
  }}
  severity="Error"
/>
