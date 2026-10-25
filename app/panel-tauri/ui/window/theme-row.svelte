<script lang="ts">
  import type { ThemeRow } from "../protocol.g.ts";
  import { glyph } from "./layout.ts";

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
  const shown = 1;
  const hidden = 0;
  const isOwn = $derived(onRemove !== null);

  function remove(): void {
    onRemove?.();
  }
</script>

<!-- The row's inset lines the list's rows and Edge's rows up. -->
<grid columns="16,*,Auto,Auto" column-spacing={8} padding="4,0,4,0" automation-properties.name={theme.name}>
  <font-icon glyph={glyph(gripGlyph)} font-size={12} opacity={isOwn ? shown : hidden} />
  <text-block class="card-title" grid.column={1} vertical-alignment="Center" text-trimming="CharacterEllipsis">{theme.name}</text-block>
  <stack-panel grid.column={2} orientation="Horizontal" spacing={4}>
    {#each theme.badges as badge}
      <border class="badge">
        <text-block class="badge-text">{badge}</text-block>
      </border>
    {/each}
  </stack-panel>
  <button grid.column={3} automation-properties.name={theme.removeLabel} is-enabled={isOwn} opacity={isOwn ? shown : hidden} onclick={remove}>
    <font-icon glyph={glyph(removeGlyph)} font-size={12} />
  </button>
</grid>

<style>
  .badge {
    background: var(--subtle-fill-color-secondary);
    border-radius: var(--control-corner-radius);
    padding: 1px 6px;
    align-self: center;
  }

  .badge-text {
    -winui-based-on: CaptionTextBlockStyle;
    color: var(--text-fill-color-secondary);
  }
</style>
