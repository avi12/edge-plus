<script lang="ts">
  import type { FooterView } from "../../protocol.g.ts";
  import { Grid, HyperlinkButton, StackPanel, TextBlock } from "winui-svelte";

  // Who made it, linked, with the version at the end.
  interface Props {
    footer: FooterView;
  }

  const { footer }: Props = $props();
</script>

<Grid class="footer">
  <StackPanel class="footer-author">
    <TextBlock class="card-description footer-by">by</TextBlock>
    <HyperlinkButton class="footer-link" navigateUri={footer.authorUri}>{footer.author}</HyperlinkButton>
  </StackPanel>
  <TextBlock class="card-description footer-version">{footer.version}</TextBlock>
</Grid>

<!-- Global: a class on a component is not an element's, so Svelte does not
     scope it. -->
<style>
  :global(.footer) {
    grid-template-columns: 1fr auto;
    margin: 0 4px;
    margin-top: 14px;
  }

  :global(.footer-author) {
    flex-direction: row;
    gap: 4px;
  }

  :global(.footer-by) {
    align-self: center;
  }

  :global(.footer-link) {
    padding: 0 2px;
  }

  :global(.footer-version) {
    grid-column: 2;
    align-self: center;
  }
</style>
