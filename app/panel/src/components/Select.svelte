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
  const uid = $props.id();

  let listbox: HTMLDivElement | undefined = $state();

  const selectedLabel = $derived(options.find(option => option.value === value)?.label ?? "");

  function choose(next: string): void {
    listbox?.hidePopover();

    if (next !== value) {
      onchange(next);
    }
  }

  function focusSelected(): void {
    const target = listbox?.querySelector<HTMLElement>("[aria-selected='true']");
    (target ?? listbox?.querySelector<HTMLElement>("[role='option']"))?.focus();
  }

  function handleToggle(event: ToggleEvent): void {
    if (event.newState === "open") {
      focusSelected();
    }
  }

  function handleKey(event: KeyboardEvent): void {
    const items = [...(listbox?.querySelectorAll<HTMLElement>("[role='option']") ?? [])];
    const index = items.findIndex(item => item === document.activeElement);
    if (event.key === "ArrowDown") {
      items[Math.min(index + 1, items.length - 1)]?.focus();
    } else if (event.key === "ArrowUp") {
      items[Math.max(index - 1, 0)]?.focus();
    } else {
      return;
    }

    event.preventDefault();
  }
</script>

<div style:--anchor="--select-{uid}" class="select-wrap">
  <button
    class="field select"
    aria-haspopup="listbox"
    aria-label={label}
    popovertarget="list-{uid}"
    type="button"
  >
    <span class="value">{selectedLabel}</span>
    <svg class="chevron" aria-hidden="true" viewBox="0 0 12 12">
      <path d="M2.5 4.5 L6 8 L9.5 4.5" fill="none" stroke="currentColor" stroke-width="1.2" />
    </svg>
  </button>
  <div
    bind:this={listbox}
    id="list-{uid}"
    class="list"
    aria-label={label}
    onkeydown={handleKey}
    ontoggle={handleToggle}
    popover="auto"
    role="listbox"
    tabindex="-1"
  >
    {#each options as option (option.value)}
      <button
        class="option"
        aria-selected={option.value === value}
        onclick={() => choose(option.value)}
        role="option"
        type="button"
      >
        {option.label}
      </button>
    {/each}
  </div>
</div>

<style>
  .select-wrap {
    display: inline-flex;
    min-width: 150px;
  }

  .select {
    display: flex;
    gap: var(--space-s);
    justify-content: space-between;
    align-items: center;
    cursor: pointer;
    anchor-name: var(--anchor);
  }

  .select:hover {
    background: var(--bg-hover);
  }

  .select:active {
    background: var(--bg-pressed);
  }

  .select:focus-visible,
  .select:has(+ .list:popover-open) {
    border-color: var(--brand);
    border-bottom-width: 2px;
    outline: none;
  }

  .chevron {
    flex: none;
    width: 12px;
    height: 12px;
    color: var(--fg-2);
    transition: rotate 120ms ease;
  }

  .select:has(+ .list:popover-open) .chevron {
    rotate: 180deg;
  }

  .list {
    overflow: auto;
    inline-size: anchor-size(width);
    margin: 0;
    margin-block-start: var(--space-xs);
    padding: var(--space-xs);
    border: 1px solid var(--stroke-2);
    border-radius: var(--radius-l);
    background: var(--bg-card);
    box-shadow: var(--shadow-pop);
    position-anchor: var(--anchor);
    position-area: bottom span-right;
    position-try-fallbacks: flip-block;
  }

  .option {
    position: relative;
    display: block;
    width: 100%;
    min-height: 32px;
    padding-inline: var(--space-m);
    border: 0;
    border-radius: var(--radius-m);
    background: transparent;
    color: var(--fg-1);
    font-family: inherit;
    font-size: 13px;
    text-align: start;
    cursor: pointer;
  }

  .option:hover,
  .option:focus-visible {
    background: var(--bg-hover);
    outline: none;
  }

  .option:active {
    background: var(--bg-pressed);
  }

  .option[aria-selected="true"] {
    background: var(--bg-hover);
  }

  .option[aria-selected="true"]::before {
    content: "";
    position: absolute;
    inset-block: 8px;
    inset-inline-start: 0;
    width: 3px;
    border-radius: var(--radius-s);
    background: var(--brand-pressed);
  }
</style>
