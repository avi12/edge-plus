<script lang="ts">
  interface Props {
    disabled?: boolean;
    label: string;
    onclick: () => void;
    variant?: "primary" | "default";
  }

  const { disabled = false, label, onclick, variant = "default" }: Props = $props();
</script>

<button class="btn {variant}" {disabled} {onclick} type="button">{label}</button>

<style>
  .btn {
    height: 32px;
    padding: 0 var(--space-m);
    border: 1px solid var(--stroke-1);
    border-radius: var(--radius-m);
    background: var(--bg-card);
    color: var(--fg-1);
    font-family: inherit;
    font-weight: 600;
    font-size: 13px;
    white-space: nowrap;
    cursor: pointer;
    transition: background 100ms ease, border-color 100ms ease;
  }

  .btn:hover {
    background: var(--bg-hover);
  }

  .btn:active {
    background: var(--bg-pressed);
  }

  .btn.primary {
    border-color: var(--brand);
    background: var(--brand);
    color: var(--on-brand);
  }

  .btn.primary:hover {
    border-color: var(--brand-hover);
    background: var(--brand-hover);
  }

  .btn.primary:active {
    border-color: var(--brand-pressed);
    background: var(--brand-pressed);
  }

  .btn:disabled {
    border-color: var(--stroke-2);
    background: var(--bg-subtle);
    color: var(--fg-disabled);
    cursor: not-allowed;
  }
</style>
