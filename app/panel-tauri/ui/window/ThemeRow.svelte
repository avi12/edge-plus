<script lang="ts">
  import type { ThemeRow } from "../protocol.g.ts";
  import { glyph } from "./layout.ts";
  import {
    Border,
    Button,
    FontIcon,
    Grid,
    StackPanel,
    TextBlock
  } from "svelte-winui";

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
<Grid automationName={theme.name} columnSpacing={8} columns="16,*,Auto,Auto" padding="4,0,4,0">
  <FontIcon fontSize={iconSize} glyph={glyph(gripGlyph)} opacity={isOwn ? shown : hidden} />
  <TextBlock
    class="card-title"
    gridColumn={1}
    text={theme.name}
    textTrimming="CharacterEllipsis"
    verticalAlignment="Center"
  />
  <StackPanel gridColumn={2} orientation="Horizontal" spacing={4}>
    {#each theme.badges as badge (badge)}
      <Border class="badge">
        <TextBlock class="badge-text" text={badge} />
      </Border>
    {/each}
  </StackPanel>
  <Button
    automationName={theme.removeLabel}
    gridColumn={3}
    isEnabled={isOwn}
    onClick={remove}
    opacity={isOwn ? shown : hidden}
  >
    <FontIcon fontSize={iconSize} glyph={glyph(removeGlyph)} />
  </Button>
</Grid>

<style>
  :global(.badge) {
    align-self: center;
    padding: 1px 6px;
    border-radius: var(--control-corner-radius);
    background: var(--subtle-fill-color-secondary);
  }

  :global(.badge-text) {
    color: var(--text-fill-color-secondary);
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
