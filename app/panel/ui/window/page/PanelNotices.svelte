<script lang="ts">
  import type { View } from "#src/protocol.g.ts";
  import { usePanel } from "#src/window/host/panel.svelte.ts";
  import { CssVisibility } from "#src/window/style/css-visibility.ts";
  import { pausedNotice } from "#src/window/wording/shared-wording.ts";
  import { InfoBar, TextBlock } from "winui-svelte/components";
  import { InfoBarSeverity } from "winui-svelte/enums";

  interface Props {
    view: View;
  }

  const { view }: Props = $props();
  const panel = usePanel();

  const buildNotice = $derived(pausedNotice(view));
</script>

<TextBlock style:visibility={view.isEdgeRunning ? CssVisibility.hidden : CssVisibility.visible} class="open-edge-hint">
  Open Edge to apply changes.
</TextBlock>
<InfoBar isClosable={false} isOpen={buildNotice.length > 0} message={buildNotice} severity={InfoBarSeverity.Warning} />
<InfoBar
  isClosable
  isOpen={panel.actionNotice.length > 0}
  message={panel.actionNotice}
  onclosed={() => {
    panel.actionNotice = "";
  }}
  severity={InfoBarSeverity.Error}
/>

<style>
  .open-edge-hint {
    margin-right: 4px;
    margin-bottom: 4px;
    margin-left: 4px;
    color: var(--text-fill-color-secondary);
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
