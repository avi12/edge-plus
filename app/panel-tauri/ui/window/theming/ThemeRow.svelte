<script lang="ts">
  import type { ThemeBadge, ThemeRow } from "#src/protocol.g.ts";
  import CardTitle from "#src/window/cards/CardTitle.svelte";
  import {
    Border,
    Button,
    FontIcon,
    Grid,
    StackPanel,
    TextBlock
  } from "winui-svelte/components";

  interface Props {
    theme: ThemeRow;
    onremove: (() => void) | null;
  }

  const { theme, onremove }: Props = $props();

  const BADGE_TEXT: Record<ThemeBadge, string> = {
    store: "Store",
    local: "Local",
    inUse: "In use",
    installedInEdge: "Installed in Edge"
  };
  const isOwn = $derived(onremove !== null);
</script>

<Grid class="theme-row" automationName={theme.name}>
  <FontIcon class="theme-row-icon" class:concealed={!isOwn} glyph="GripperBarHorizontal" />
  <Border class="theme-row-name">
    <CardTitle>{theme.name}</CardTitle>
  </Border>
  <StackPanel class="theme-row-badges">
    {#each theme.badges as badge (badge)}
      <Border class="badge">
        <TextBlock class="badge-text">{BADGE_TEXT[badge]}</TextBlock>
      </Border>
    {/each}
  </StackPanel>
  <Button
    class="theme-row-remove"
    automationId="remove-{theme.id}"
    automationName="Remove {theme.name}"
    isEnabled={isOwn}
    onclick={() => onremove?.()}
  >
    <FontIcon class="theme-row-icon" glyph="Cancel" />
  </Button>
</Grid>

<style>
  .theme-row {
    grid-template-columns: 16px 1fr auto auto;
    padding: 0 4px;
    column-gap: 8px;
  }

  .theme-row-icon {
    font-size: 12px;
  }

  .concealed {
    opacity: 0;
  }

  .theme-row-name {
    grid-column: 2;
    align-self: center;
  }

  .theme-row-badges {
    flex-direction: row;
    grid-column: 3;
    gap: 4px;
  }

  .theme-row-remove {
    grid-column: 4;
  }

  .badge {
    align-self: center;
    padding: 1px 6px;
    border-radius: var(--control-corner-radius);
    background: var(--subtle-fill-color-secondary);
  }

  .badge-text {
    color: var(--text-fill-color-secondary);
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
