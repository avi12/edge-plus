<script lang="ts">
  import Button from "@/components/Button.svelte";
  import CardHeader from "@/components/CardHeader.svelte";
  import Select from "@/components/Select.svelte";
  import Switch from "@/components/Switch.svelte";
  import TextInput from "@/components/TextInput.svelte";
  import { LocalThemeKind } from "@/lib/bridge";
  import type { Mode, ThemeSource, Theming } from "@/lib/bridge";

  interface Props {
    description: string;
    disabled?: boolean;
    enabled: boolean;
    icon: string;
    onAddLocal: (kind: LocalThemeKind) => Promise<void>;
    onAddStore: (url: string) => Promise<void>;
    onRemove: (id: string) => Promise<void>;
    onSetMode: (mode: Mode, id: string) => Promise<void>;
    onToggle: (value: boolean) => void;
    theming: Theming;
  }

  const {
    description,
    disabled = false,
    enabled,
    icon,
    onAddLocal,
    onAddStore,
    onRemove,
    onSetMode,
    onToggle,
    theming
  }: Props = $props();

  let url = $state("");
  let error = $state("");
  let busy = $state(false);

  const options = $derived([
    {
      value: "",
      label: "Default theme"
    },
    ...theming.themes.map(theme => ({
      value: theme.id,
      label: theme.name
    }))
  ]);

  function sourceLabel(source: ThemeSource): string {
    if (source === "local") {
      return "Local";
    }

    return "Store";
  }

  async function run(action: () => Promise<void>): Promise<void> {
    error = "";
    busy = true;
    try {
      await action();
    } catch (failure) {
      error = String(failure);
    } finally {
      busy = false;
    }
  }

  async function addStore(): Promise<void> {
    if (url.trim().length === 0) {
      return;
    }

    await run(async () => {
      await onAddStore(url.trim());
      url = "";
    });
  }

  function addLocal(kind: LocalThemeKind): void {
    void run(async () => {
      await onAddLocal(kind);
    });
  }

  function remove(id: string): void {
    void run(async () => {
      await onRemove(id);
    });
  }

  function setMode(mode: Mode, id: string): void {
    void run(async () => {
      await onSetMode(mode, id);
    });
  }
</script>

<section class="card">
  <CardHeader {description} {icon} title="Theme by Windows light and dark">
    <Switch
      checked={enabled}
      {disabled}
      label="Theme by Windows light and dark"
      onchange={onToggle}
    />
  </CardHeader>

  <div class="panel" class:dimmed={!enabled}>
    <div class="add-row">
      <TextInput
        onEnter={addStore}
        onInput={value => (url = value)}
        placeholder="Paste a Chrome Web Store or Edge Add-ons link"
        value={url}
      />
      <Button disabled={busy} label="Add" onclick={addStore} variant="primary" />
      <Button disabled={busy} label="Local theme file…" onclick={() => addLocal(LocalThemeKind.File)} />
      <Button disabled={busy} label="Local theme folder…" onclick={() => addLocal(LocalThemeKind.Folder)} />
    </div>

    {#if error.length > 0}
      <p class="error">{error}</p>
    {/if}

    <div class="assign">
      <span class="assign-label">When Windows is light</span>
      <Select
        label="Light-mode theme"
        onchange={id => setMode("light", id)}
        {options}
        value={theming.light}
      />
    </div>
    <div class="assign">
      <span class="assign-label">When Windows is dark</span>
      <Select
        label="Dark-mode theme"
        onchange={id => setMode("dark", id)}
        {options}
        value={theming.dark}
      />
    </div>

    <ul class="themes">
      {#each theming.themes as theme (theme.id)}
        <li class="theme">
          <span class="name">{theme.name}</span>
          <span class="badge">{sourceLabel(theme.source)}</span>
          <button
            class="remove"
            aria-label={`Remove ${theme.name}`}
            onclick={() => remove(theme.id)}
            type="button"
          >
            ✕
          </button>
        </li>
      {:else}
        <li class="empty">No themes yet — paste a store link or add a local theme (.crx, .zip or folder).</li>
      {/each}
    </ul>
  </div>
</section>

<style>
  .card {
    padding: var(--space-l);
    border: 1px solid var(--stroke-2);
    border-radius: 8px;
    background: var(--bg-card);
    box-shadow: var(--shadow-card);
  }

  .panel {
    margin-top: var(--space-l);
    padding-top: var(--space-l);
    border-top: 1px solid var(--stroke-2);
    transition: opacity 140ms ease;
  }

  .panel.dimmed {
    opacity: 50%;
  }

  .add-row {
    display: flex;
    gap: var(--space-s);
    align-items: center;
  }

  .add-row :global(.input) {
    flex: 1 1 auto;
  }

  .error {
    margin-block: var(--space-s) 0;
    margin-inline: 0;
    padding: var(--space-s) var(--space-m);
    border-radius: var(--radius-m);
    background: var(--danger-bg);
    color: var(--danger-fg);
    font-size: 12.5px;
  }

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

  .remove:hover {
    background: var(--bg-hover);
    color: var(--danger-fg);
  }

  .empty {
    padding: var(--space-s) 0;
    color: var(--fg-3);
    font-size: 12.5px;
  }

  .assign {
    display: flex;
    gap: var(--space-m);
    justify-content: space-between;
    align-items: center;
    margin-top: var(--space-m);
  }

  .assign-label {
    font-size: 13px;
  }
</style>
