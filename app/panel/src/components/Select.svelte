<script lang="ts">
  interface SelectOption {
    label: string;
    value: string;
  }

  interface Props {
    label: string;
    onchange: (value: string) => void;
    options: SelectOption[];
    value: string;
  }

  const { label, onchange, options, value }: Props = $props();

  function handle(event: Event): void {
    if (event.currentTarget instanceof HTMLSelectElement) {
      onchange(event.currentTarget.value);
    }
  }
</script>

<div class="select-wrap">
  <select class="select" aria-label={label} onchange={handle}>
    {#each options as option (option.value)}
      <option selected={option.value === value} value={option.value}>{option.label}</option>
    {/each}
  </select>
  <svg class="chevron" aria-hidden="true" viewBox="0 0 12 12">
    <path d="M2.5 4.5 L6 8 L9.5 4.5" fill="none" stroke="currentColor" stroke-width="1.2" />
  </svg>
</div>

<style>
  .select-wrap {
    position: relative;
    display: inline-flex;
    align-items: center;
    min-width: 150px;
  }

  .select {
    appearance: none;
    width: 100%;
    height: 32px;
    padding-block: 0;
    padding-inline: var(--space-m) 28px;
    border: 1px solid var(--stroke-1);
    border-bottom-color: var(--stroke-accessible);
    border-radius: var(--radius-m);
    background: var(--bg-card);
    color: var(--fg-1);
    font-family: inherit;
    font-size: 13px;
    cursor: pointer;
  }

  .select:focus {
    border-color: var(--brand);
    border-bottom-width: 2px;
    outline: none;
  }

  .chevron {
    position: absolute;
    right: var(--space-m);
    width: 12px;
    height: 12px;
    color: var(--fg-2);
    pointer-events: none;
  }
</style>
