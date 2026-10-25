<script lang="ts">
  import type { ThemeRow } from "../protocol.g.ts";
  import { glyph } from "./layout.ts";
  import { Border, Button, FontIcon, Grid, StackPanel, TextBlock } from "svelte-winui";

  // One theme: a grip, its name, its badges and a remove button. `onRemove` is
  // null for a theme installed in Edge, whose grip and remove button are only
  // there invisibly, so every row's columns line up.
  interface Props {
    theme: ThemeRow;
    onRemove: (() => void) | null;
  }

  const { theme, onRemove }: Props = $props();

  const removeGlyph = 0xE711;
  const gripGlyph = 0xE76F;
  const iconSize = 12;
  const shown = 1;
  const hidden = 0;
  const isOwn = $derived(onRemove !== null);

  function remove(): void {
    onRemove?.();
  }
</script>

<!-- The row's inset lines the list's rows and Edge's rows up. -->
<Grid columns="16,*,Auto,Auto" columnSpacing={8} padding="4,0,4,0" automationName={theme.name}>
  <FontIcon glyph={glyph(gripGlyph)} fontSize={iconSize} opacity={isOwn ? shown : hidden} />
  <TextBlock class="card-title" gridColumn={1} verticalAlignment="Center" textTrimming="CharacterEllipsis" text={theme.name} />
  <StackPanel gridColumn={2} orientation="Horizontal" spacing={4}>
    {#each theme.badges as badge}
      <Border class="badge">
        <TextBlock class="badge-text" text={badge} />
      </Border>
    {/each}
  </StackPanel>
  <Button gridColumn={3} automationName={theme.removeLabel} isEnabled={isOwn} opacity={isOwn ? shown : hidden} onClick={remove}>
    <FontIcon glyph={glyph(removeGlyph)} fontSize={iconSize} />
  </Button>
</Grid>

<style>
  :global(.badge) {
    background: var(--subtle-fill-color-secondary);
    border-radius: var(--control-corner-radius);
    padding: 1px 6px;
    align-self: center;
  }

  :global(.badge-text) {
    -winui-based-on: CaptionTextBlockStyle;
    color: var(--text-fill-color-secondary);
  }
</style>
