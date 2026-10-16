<script lang="ts">
  import FeatureCard from "@/components/FeatureCard.svelte";
  import ThemingCard from "@/components/ThemingCard.svelte";
  import type { AppState, Mode } from "@/lib/bridge";
  import * as bridge from "@/lib/bridge";
  import { open } from "@tauri-apps/plugin-dialog";
  import { onMount } from "svelte";

  const splitIcon =
    `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">` +
      `<rect x="3" y="4" width="18" height="16" rx="1.5"/><line x1="12" y1="4" x2="12" y2="20"/></svg>`;
  const themeIcon =
    `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">` +
      `<circle cx="12" cy="12" r="8"/><path d="M12 4 A8 8 0 0 0 12 20 Z" fill="currentColor" stroke="none"/></svg>`;
  const shieldIcon =
    `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">` +
      `<path d="M12 3 L20 6 V11 C20 16 16 20 12 21 C8 20 4 16 4 11 V6 Z"/></svg>`;

  const splitDescription =
    "Grows Edge's own split past two panes, loaded straight into Edge. Add panes above, below, " +
      "left or right; drag the dividers; Shift+Alt+arrow moves between panes. Turning it on injects " +
      "the module, off removes it — no restart. Open Edge first.";
  const themeDescription =
    "Use one theme when Windows is light and another when it is dark, switched live. Add themes " +
      "from the Chrome Web Store, Edge Add-ons or a local file.";
  const devModeDescription =
    "Stops Edge asking to turn off developer-mode extensions when it starts. Takes effect from " +
      "Edge's next start.";

  let state = $state<AppState | null>(null);

  onMount(async () => {
    state = await bridge.loadState();
  });

  async function onSplit(value: boolean): Promise<void> {
    state = await bridge.setSplit(value);
  }

  async function onTheming(value: boolean): Promise<void> {
    state = await bridge.setTheming(value);
  }

  async function onQuietDevMode(value: boolean): Promise<void> {
    state = await bridge.setQuietDevMode(value);
  }

  async function onAddStore(url: string): Promise<void> {
    state = await bridge.addStoreTheme(url);
  }

  async function onAddLocal(): Promise<void> {
    const picked = await open({
      multiple: false,
      filters: [{
        name: "Edge theme",
        extensions: ["crx"]
      }]
    });
    if (typeof picked === "string") {
      state = await bridge.addLocalTheme(picked);
    }
  }

  async function onRemove(id: string): Promise<void> {
    state = await bridge.removeTheme(id);
  }

  async function onSetMode(mode: Mode, id: string): Promise<void> {
    state = await bridge.setModeTheme(mode, id);
  }
</script>

{#if state}
  <main class="app">
    <header class="title">
      <h1>Fix Edge</h1>
      {#if !state.edgeRunning}
        <p class="hint">Open Edge to apply changes.</p>
      {/if}
    </header>

    <FeatureCard
      checked={state.features.nativeSplit}
      description={splitDescription}
      icon={splitIcon}
      onchange={onSplit}
      title="Split into any grid"
    />

    <ThemingCard
      description={themeDescription}
      enabled={state.features.theming}
      icon={themeIcon}
      {onAddLocal}
      {onAddStore}
      {onRemove}
      {onSetMode}
      onToggle={onTheming}
      theming={state.theming}
    />

    <FeatureCard
      checked={state.features.quietDevMode}
      description={devModeDescription}
      icon={shieldIcon}
      onchange={onQuietDevMode}
      title="Hide the developer-mode prompt"
    />
  </main>
{/if}

<style>
  .app {
    display: flex;
    flex-direction: column;
    gap: var(--space-m);
    max-width: 560px;
    margin: 0 auto;
    padding-block: var(--space-xl) var(--space-xxl);
    padding-inline: var(--space-l);
  }

  .title {
    padding-block: var(--space-s) var(--space-xs);
    padding-inline: 0;
  }

  h1 {
    margin: 0;
    font-weight: 600;
    font-size: 20px;
  }

  .hint {
    margin-block: var(--space-xs) 0;
    margin-inline: 0;
    color: var(--fg-3);
    font-size: 12.5px;
  }
</style>
