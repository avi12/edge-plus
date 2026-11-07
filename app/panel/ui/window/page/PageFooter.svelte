<script lang="ts">
  import type { FooterView } from "#src/protocol.g.ts";
  import { Grid, HyperlinkButton, StackPanel, TextBlock } from "winui-svelte/components";

  interface Props {
    footer: FooterView;
  }

  const { footer }: Props = $props();
</script>

<Grid class="footer">
  <StackPanel class="footer-author">
    <TextBlock class="footer-by">by</TextBlock>
    <HyperlinkButton class="footer-link" navigateUri={footer.authorUri}>{footer.author}</HyperlinkButton>
  </StackPanel>
  <TextBlock class="footer-version">{footer.version}</TextBlock>
</Grid>

<style>
  .footer {
    grid-template-columns: 1fr auto;
    margin-top: 14px;
    margin-right: 4px;
    margin-left: 4px;
  }

  .footer-author {
    flex-direction: row;
    gap: 4px;
  }

  .footer-by,
  .footer-version {
    align-self: center;
    color: var(--text-fill-color-secondary);
    -winui-based-on: CaptionTextBlockStyle;
  }

  .footer-link {
    padding: 0 2px;
  }

  .footer-version {
    grid-column: 2;
  }
</style>
