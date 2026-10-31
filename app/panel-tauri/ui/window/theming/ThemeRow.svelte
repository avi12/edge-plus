<script lang="ts">
  import type { ThemeBadge, ThemeRow } from "../../protocol.g.ts";
  import { glyph } from "../style/layout.ts";
  import {
    Border,
    Button,
    FontIcon,
    Grid,
    StackPanel,
    TextBlock
  } from "winui-svelte";

  // One theme: a grip, its name, its badges and a remove button. `onRemove` is
  // null for a theme installed in Edge, whose grip and remove button are only
  // there invisibly, so every row's columns line up.
  interface Props {
    theme: ThemeRow;
    onRemove: (() => void) | null;
  }

  const { theme, onRemove }: Props = $props();

  const badgeText: Record<ThemeBadge, string> = {
    store: "Store",
    local: "Local",
    inUse: "In use",
    installedInEdge: "Installed in Edge"
  };
  const removeGlyph = 0xE711;
  const gripGlyph = 0xE76F;
  const isOwn = $derived(onRemove !== null);
  const ownOnly = $derived(isOwn ? "" : "concealed");

  function remove(): void {
    onRemove?.();
  }
</script>

<Grid class="theme-row" automationName={theme.name}>
  <FontIcon class="theme-row-icon {ownOnly}" glyph={glyph(gripGlyph)} />
  <TextBlock class="card-title theme-row-name">{theme.name}</TextBlock>
  <StackPanel class="theme-row-badges">
    {#each theme.badges as badge (badge)}
      <Border class="badge">
        <TextBlock class="badge-text">{badgeText[badge]}</TextBlock>
      </Border>
    {/each}
  </StackPanel>
  <Button
    class="theme-row-remove {ownOnly}"
    automationName="Remove {theme.name}"
    isEnabled={isOwn}
    onClick={remove}
  >
    <FontIcon class="theme-row-icon" glyph={glyph(removeGlyph)} />
  </Button>
</Grid>

<style>
  /* The row's inset lines the list's rows and Edge's rows up. */
  :global(.theme-row) {
    grid-template-columns: 16px 1fr auto auto;
    padding: 0 4px;
    column-gap: 8px;
  }

  :global(.theme-row-icon) {
    font-size: 12px;
  }

  :global(.theme-row-name) {
    grid-column: 2;
    align-self: center;
    text-overflow: ellipsis;
  }

  :global(.theme-row-badges) {
    flex-direction: row;
    grid-column: 3;
    gap: 4px;
  }

  :global(.theme-row-remove) {
    grid-column: 4;
  }

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
