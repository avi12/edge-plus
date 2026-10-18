<script lang="ts" module>
  /** One theme to show: only the panel's own themes can be removed. */
  export interface ThemeRow {
    id: string;
    name: string;
    badges: string[];
    isRemovable: boolean;
  }
</script>

<script lang="ts">
  interface Props {
    onRemove: (id: string) => void;
    rows: ThemeRow[];
  }

  const { onRemove, rows }: Props = $props();
</script>

<ul class="themes">
  {#each rows as row (row.id)}
    <li class="theme">
      <span class="name">{row.name}</span>
      {#each row.badges as badge (badge)}
        <span class="badge">{badge}</span>
      {/each}
      {#if row.isRemovable}
        <button
          class="remove"
          aria-label={`Remove ${row.name}`}
          onclick={() => onRemove(row.id)}
          type="button"
        >
          ✕
        </button>
      {:else}
        <span class="remove-space"></span>
      {/if}
    </li>
  {:else}
    <li class="empty">No themes yet — paste a store link or add a local theme (.crx, .zip or folder).</li>
  {/each}
</ul>

<style>
  .themes {
    margin: var(--space-m) 0;
    padding: 0;
    list-style: none;
  }

  .theme {
    display: flex;
    gap: var(--space-s);
    align-items: center;
    padding: var(--space-s) 0;
    border-bottom: 1px solid var(--stroke-2);
  }

  .name {
    flex: 1 1 auto;
    overflow: hidden;
    min-width: 0;
    font-size: 13px;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .badge {
    padding: 1px var(--space-s);
    border: 1px solid var(--stroke-2);
    border-radius: var(--radius-s);
    color: var(--fg-3);
    font-size: 11px;
    text-transform: uppercase;
  }

  .remove {
    width: 24px;
    height: 24px;
    border: none;
    border-radius: var(--radius-m);
    background: transparent;
    color: var(--fg-3);
    font-size: 12px;
    cursor: pointer;
  }

  .remove-space {
    flex: 0 0 24px;
  }

  .remove:hover {
    background: var(--bg-hover);
    color: var(--danger-fg);
  }

  .empty {
    padding: var(--space-s) 0;
    color: var(--fg-3);
    font-size: 12.5px;
  }
</style>
