<script lang="ts">
  import type { FooterView } from "../protocol.g.ts";
  import { Uri } from "./winui.ts";

  // Who made it, linked, with the version at the end.
  const { footer }: { footer: FooterView } = $props();
</script>

<grid margin="4,14,4,0" columns="*,Auto">
  <stack-panel orientation="Horizontal" spacing="4">
    <text-block class="CardDescription" vertical-alignment="Center">{footer.by}</text-block>
    <hyperlink-button navigate-uri={new Uri(footer.authorUri)} padding="2,0,2,0">{footer.author}</hyperlink-button>
  </stack-panel>
  <text-block class="CardDescription" vertical-alignment="Center" grid.column={1}>{footer.version}</text-block>
</grid>
