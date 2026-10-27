<script lang="ts">
  import type { Snippet } from "svelte";
  import { TextBlock } from "winui-svelte";

  // A section's header, its text written as the element's children.
  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
</script>

<TextBlock class="section-header">{@render children()}</TextBlock>

<!-- Global: a class on a component is not an element's, so Svelte does not
     scope it. The left margin lines the header up with the cards' text, as
     Windows Settings does. -->
<style>
  :global(.section-header) {
    margin-bottom: 4px;
    margin-left: 4px;
    -winui-based-on: BodyStrongTextBlockStyle;
  }
</style>
