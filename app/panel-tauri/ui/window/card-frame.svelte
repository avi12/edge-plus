<script lang="ts">
  import type { CardWording } from "../protocol.g.ts";
  import { glyph } from "./layout.ts";
  import type { FrameworkElement } from "./winui.ts";
  import { Visibility } from "./winui.ts";

  // The Windows Settings card, built by hand from WinUI's own controls rather
  // than a toolkit's SettingsCard (no UI library, to keep the supply chain
  // small): an icon, a title over a description, and one control at the end.
  // A feature card puts a switch there, the Inject card a button; a card may
  // carry a body under its words.
  interface Props {
    wording: CardWording;
    control: FrameworkElement;
    note?: string;
    body?: FrameworkElement;
  }

  const { wording, control, note = "", body }: Props = $props();
</script>

<!-- The right margin on the words keeps a wrapped description off the
     control, as in Windows Settings. -->
<border class="Card">
  <grid columns="Auto,*,Auto" rows="Auto,Auto">
    <font-icon glyph={glyph(wording.glyph)} font-size={20} margin="0,0,16,0" vertical-alignment="Center" />
    <stack-panel grid.column={1} margin="0,0,16,0" vertical-alignment="Center">
      <text-block class="CardTitle">{wording.title}</text-block>
      <text-block class="CardDescription">{wording.description}</text-block>
      <text-block class="CardNote" visibility={note.length > 0 ? Visibility.Visible : Visibility.Collapsed}>{note}</text-block>
    </stack-panel>
    <host element={control} grid.column={2} vertical-alignment="Center" />
    {#if body}
      <host element={body} grid.row={1} grid.column={1} grid.column-span={2} margin="0,12,0,0" />
    {/if}
  </grid>
</border>
