<script lang="ts">
  import type { View } from "../protocol.g.ts";
  import { usePanel } from "./panel.svelte.ts";
  import { InfoBar, TextBlock } from "svelte-winui";

  // What the page says above the cards: the hint to open Edge, the warning
  // that Edge's build is not the one Edge+ was made for, and the error of an
  // action that failed.
  const { view }: { view: View } = $props();
  const panel = usePanel();
</script>

<TextBlock class="card-description" margin="4,0,4,4" visibility={view.openEdgeHint === null ? "Collapsed" : "Visible"} text={view.openEdgeHint ?? ""} />
<InfoBar isClosable={false} severity="Warning" message={view.buildNotice} isOpen={view.buildNotice.length > 0} />
<InfoBar
  isClosable={true}
  severity="Error"
  message={panel.actionNotice}
  isOpen={panel.actionNotice.length > 0}
  onClosed={() => {
    panel.actionNotice = "";
  }}
/>
