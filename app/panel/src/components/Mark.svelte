<script lang="ts">
  // A small inline-SVG mark beside a label; nothing when there is no icon.
  interface Props {
    icon?: string;
  }

  const { icon = "" }: Props = $props();
</script>

{#if icon}
  <span class="mark">{@html icon}</span>
{/if}

<style>
  .mark {
    display: inline-flex;
    flex: none;
    width: 16px;
    height: 16px;
  }

  .mark :global(svg) {
    width: 100%;
    height: 100%;
  }
</style>
