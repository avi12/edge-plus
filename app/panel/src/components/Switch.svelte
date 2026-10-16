<script lang="ts">
  // A Fluent 2 toggle, hand-built from a visually-hidden checkbox and a styled
  // track + thumb, so no component library is pulled in.
  interface Props {
    checked: boolean;
    disabled?: boolean;
    label: string;
    onchange: (value: boolean) => void;
  }

  const { checked, disabled = false, label, onchange }: Props = $props();

  function handle(event: Event): void {
    if (event.currentTarget instanceof HTMLInputElement) {
      onchange(event.currentTarget.checked);
    }
  }
</script>

<label class="switch" class:disabled>
  <input aria-label={label} {checked} {disabled} onchange={handle} type="checkbox" />
  <span class="track">
    <span class="thumb"></span>
  </span>
</label>

<style>
  .switch {
    position: relative;
    display: inline-flex;
    flex: 0 0 auto;
    align-items: center;
    cursor: pointer;
  }

  .switch.disabled {
    cursor: not-allowed;
  }

  input {
    position: absolute;
    width: 100%;
    height: 100%;
    margin: 0;
    opacity: 0%;
    cursor: inherit;
  }

  .track {
    display: inline-flex;
    align-items: center;
    width: 40px;
    height: 20px;
    padding: 0 2px;
    border: 1px solid var(--stroke-accessible);
    border-radius: 10px;
    background: transparent;
    transition: background 120ms ease, border-color 120ms ease;
  }

  .thumb {
    width: 14px;
    height: 14px;
    border-radius: 50%;
    background: var(--stroke-accessible);
    transition: transform 120ms ease, background 120ms ease;
  }

  /* Ordered by ascending specificity (no-descending-specificity). */
  input:checked ~ .track {
    border-color: var(--brand);
    background: var(--brand);
  }

  input:focus-visible ~ .track {
    outline: 2px solid var(--focus);
    outline-offset: 2px;
  }

  .switch:hover .track {
    border-color: var(--fg-2);
  }

  .switch:hover .thumb {
    background: var(--fg-2);
  }

  .switch.disabled .track {
    border-color: var(--fg-disabled);
  }

  .switch.disabled .thumb {
    background: var(--fg-disabled);
  }

  input:checked ~ .track .thumb {
    background: var(--on-brand);
    transform: translateX(20px);
  }

  .switch:hover input:checked ~ .track {
    border-color: var(--brand-hover);
    background: var(--brand-hover);
  }

  .switch.disabled input:checked ~ .track {
    border-color: var(--fg-disabled);
    background: var(--fg-disabled);
  }
</style>
