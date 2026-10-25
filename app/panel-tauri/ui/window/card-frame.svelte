<script lang="ts">
  import { glyph } from "./layout.ts";
  import type { CardFrameProps } from "./card-frame.d.svelte.ts";
  import { Visibility } from "./winui.ts";

  // The Windows Settings card, built by hand from WinUI's own controls rather
  // than a toolkit's SettingsCard (no UI library, to keep the supply chain
  // small): an icon, a title over a description, and one control at the end.
  // A feature card puts a switch there, the Inject card a button; a card may
  // carry a body under its words.
  const { wording, note = "", isShown = true, end, body }: CardFrameProps = $props();
</script>

<!-- The right margin on the words keeps a wrapped description off the
     control, as in Windows Settings. The end control and the body sit in a
     bare border each, which places them in the grid. -->
<border class="card" visibility={isShown ? Visibility.Visible : Visibility.Collapsed}>
  <grid columns="Auto,*,Auto" rows="Auto,Auto">
    <font-icon glyph={glyph(wording.glyph)} font-size={20} margin="0,0,16,0" vertical-alignment="Center" />
    <stack-panel grid.column={1} margin="0,0,16,0" vertical-alignment="Center">
      <text-block class="card-title">{wording.title}</text-block>
      <text-block class="card-description">{wording.description}</text-block>
      <text-block class="note" visibility={note.length > 0 ? Visibility.Visible : Visibility.Collapsed}>{note}</text-block>
    </stack-panel>
    <border grid.column={2} vertical-alignment="Center">
      {@render end()}
    </border>
    {#if body}
      <border grid.row={1} grid.column={1} grid.column-span={2} margin="0,12,0,0">
        {@render body()}
      </border>
    {/if}
  </grid>
</border>

<!-- Every colour is a WinUI theme resource, so light and dark follow Windows. -->
<style>
  .card {
    background: var(--card-background-fill-color-default);
    border: 1px solid var(--card-stroke-color-default);
    border-radius: var(--control-corner-radius);
    padding: 12px 16px;
    min-height: 68px;
  }

  .note {
    -winui-based-on: CaptionTextBlockStyle;
    color: var(--system-fill-color-caution);
    text-wrap: wrap;
  }
</style>
