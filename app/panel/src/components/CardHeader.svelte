<script lang="ts">
  // The icon + title + description row shared by every feature card. The
  // trailing control (a switch, a select) is the caller's, passed as children.
  import type { Snippet } from "svelte";

  interface Props {
    icon: string;
    title: string;
    description: string;
    // Why the card's control is locked; empty when it is not.
    note?: string;
    children: Snippet;
  }

  const { icon, title, description, note = "", children }: Props = $props();
</script>

<div class="head">
  <div class="icon">{@html icon}</div>
  <div class="body">
    <h2>{title}</h2>
    <p>{description}</p>
    {#if note.length > 0}
      <p class="note">{note}</p>
    {/if}
  </div>
  {@render children()}
</div>

<style>
  .head {
    display: flex;
    gap: var(--space-l);
    align-items: flex-start;
  }

  .icon {
    display: flex;
    flex: 0 0 auto;
    color: var(--brand);
  }

  .icon :global(svg) {
    width: 24px;
    height: 24px;
  }

  .body {
    flex: 1 1 auto;
    min-width: 0;
  }

  h2 {
    margin-block: 0 var(--space-xs);
    margin-inline: 0;
    font-weight: 600;
    font-size: 15px;
  }

  .note {
    margin-block-start: var(--space-s);
    color: var(--danger-fg);
    font-weight: 600;
  }

  p {
    margin: 0;
    color: var(--fg-3);
    font-size: 12.5px;
    line-height: 1.5;
  }
</style>
