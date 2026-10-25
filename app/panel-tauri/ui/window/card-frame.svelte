<script lang="ts">
  import type { CardWording } from "../protocol.g.ts";
  import { glyph } from "./layout.ts";
  import type { Snippet } from "svelte";
  import { Border, FontIcon, Grid, StackPanel, TextBlock } from "svelte-winui";

  // The Windows Settings card, built by hand from WinUI's own controls rather
  // than a toolkit's SettingsCard (no UI library, to keep the supply chain
  // small): an icon, a title over a description, and one control at the end.
  // A feature card puts a switch there, the Inject card a button; a card may
  // carry a body under its words.
  interface Props {
    wording: CardWording;
    note?: string;
    end: Snippet;
    body?: Snippet;
  }

  const { wording, note = "", end, body }: Props = $props();
</script>

<!-- The right margin on the words keeps a wrapped description off the
     control, as in Windows Settings. The end control and the body sit in a
     bare border each, which places them in the grid. -->
<Border class="card">
  <Grid columns="Auto,*,Auto" rows="Auto,Auto">
    <FontIcon glyph={glyph(wording.glyph)} fontSize={20} margin="0,0,16,0" verticalAlignment="Center" />
    <StackPanel gridColumn={1} margin="0,0,16,0" verticalAlignment="Center">
      <TextBlock class="card-title" text={wording.title} />
      <TextBlock class="card-description" text={wording.description} />
      <TextBlock class="note" visibility={note.length > 0 ? "Visible" : "Collapsed"} text={note} />
    </StackPanel>
    <Border gridColumn={2} verticalAlignment="Center">
      {@render end()}
    </Border>
    {#if body}
      <Border gridRow={1} gridColumn={1} gridColumnSpan={2} margin="0,12,0,0">
        {@render body()}
      </Border>
    {/if}
  </Grid>
</Border>

<!-- Every colour is a WinUI theme resource, so light and dark follow Windows.
     A class on a component is not an element's, so Svelte does not scope it:
     these rules are global, with names no other component uses. -->
<style>
  :global(.card) {
    background: var(--card-background-fill-color-default);
    border: 1px solid var(--card-stroke-color-default);
    border-radius: var(--control-corner-radius);
    padding: 12px 16px;
    min-height: 68px;
  }

  :global(.note) {
    -winui-based-on: CaptionTextBlockStyle;
    color: var(--system-fill-color-caution);
    text-wrap: wrap;
  }
</style>
