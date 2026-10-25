<script lang="ts">
  import type { ThemeOption } from "../protocol.g.ts";

  // The theme for one Windows mode: Edge's default theme, then the user's
  // themes, then Edge's own.
  interface Props {
    options: ThemeOption[];
  }

  const { options }: Props = $props();
</script>

<!-- Keyed by the whole option list, so a changed list gets new items: moving
     a ComboBoxItem by removing and re-inserting the same element throws
     (E_INVALIDARG). -->
<combo-box>
  {#key options}
    {#each options as option}
      <combo-box-item>{option.label}</combo-box-item>
    {/each}
  {/key}
</combo-box>
