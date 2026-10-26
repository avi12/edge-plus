<script lang="ts">
  import type { FooterView } from "../protocol.g.ts";
  import { Grid, HyperlinkButton, StackPanel, TextBlock } from "svelte-winui";

  // Who made it, linked, with the version at the end.
  const { footer }: { footer: FooterView } = $props();
</script>

<Grid margin="4,14,4,0" columns="*,Auto">
  <StackPanel orientation="Horizontal" spacing={4}>
    <TextBlock class="card-description" verticalAlignment="Center" text={footer.by} />
    <HyperlinkButton navigateUri={footer.authorUri} padding="2,0,2,0">{footer.author}</HyperlinkButton>
  </StackPanel>
  <TextBlock class="card-description" verticalAlignment="Center" gridColumn={1} text={footer.version} />
</Grid>
