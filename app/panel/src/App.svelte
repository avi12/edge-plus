<script lang="ts">
  import FeatureCard from "@/components/FeatureCard.svelte";
  import ThemingCard from "@/components/ThemingCard.svelte";
  import { FIND_DESCRIPTION, FIND_TITLE } from "@/lib/assistantNames";
  import type { AppState, Mode } from "@/lib/bridge";
  import * as bridge from "@/lib/bridge";
  import { Feature, LocalThemeKind } from "@/lib/bridge";
  import { describeHeldOff, describeUnsupported, describeUpdate } from "@/lib/edgeBuild";
  import { listen } from "@tauri-apps/api/event";
  import { getCurrentWebview } from "@tauri-apps/api/webview";
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
  // Chrome's handler icon: two overlapping diamonds.
  const handlerIcon =
    `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">` +
      `<path d="M9 5 L16 12 L9 19 L2 12 Z"/><path d="M15 5 L22 12 L15 19 L12 16"/></svg>`;

  // A magnifying glass with a RegExp asterisk.
  const findIcon =
    `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">` +
      `<circle cx="10" cy="10" r="6"/><line x1="14.5" y1="14.5" x2="20" y2="20"/>` +
      `<line x1="10" y1="7.5" x2="10" y2="12.5"/><line x1="7.8" y1="8.7" x2="12.2" y2="11.3"/>` +
      `<line x1="12.2" y1="8.7" x2="7.8" y2="11.3"/></svg>`;

  // A page with a half-filled sun/moon disc.
  const webUiIcon =
    `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">` +
      `<rect x="3" y="4" width="18" height="16" rx="1.5"/><line x1="3" y1="8" x2="21" y2="8"/>` +
      `<circle cx="12" cy="14" r="3"/><path d="M12 11 A3 3 0 0 0 12 17 Z" fill="currentColor" stroke="none"/></svg>`;

  // A power button.
  const startupIcon =
    `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">` +
      `<path d="M7 6.5 A8 8 0 1 0 17 6.5"/><line x1="12" y1="3" x2="12" y2="12"/></svg>`;

  const startupDescription =
    "Opens Edge+ minimized when you sign in to Windows, so your settings apply without opening " +
      "it. Launch Edge+ again to bring its window back. Also shown in Task Manager under Startup apps.";
  const splitDescription =
    "Grows Edge's own split past two panes, loaded straight into Edge. Add panes above, below, " +
      "left or right; drag the dividers; Shift+Alt+arrow moves between panes. Turning it on injects " +
      "the module, off removes it — no restart. Open Edge first.";
  const themeDescription =
    "Use one theme when Windows is light and another when it is dark, switched live. Add themes " +
      "from the Chrome Web Store, Edge Add-ons or a local file.";
  const webUiDescription =
    "Makes edge://settings and edge://extensions light or dark to match the theme Edge is " +
      "showing, not Windows: they follow your theme's colors, so a dark theme gives dark pages " +
      "even when Windows is light, and Theming's live swap changes them too. A profile on Edge's " +
      "own default theme keeps following Edge's Appearance setting. Web pages see the same " +
      "light or dark. Live, no restart. Open Edge first.";
  const devModeDescription =
    "Stops Edge asking to turn off developer-mode extensions when it starts. Takes effect from " +
      "Edge's next start.";
  const handlersDescription =
    "When a site such as Gmail offers to open a kind of link (email, calendar), Edge shows its " +
      "handler icon in the address bar, as Chrome does. Closing that bubble or choosing \"Just this " +
      "time\" no longer blocks the site for good; only Done on Ignore, or Block, does. Turning this " +
      "on (or opening Edge with it on) lifts every block so far, so blocked sites can ask again. " +
      "Live, no restart.";

  let appState = $state<AppState | null>(null);
  let isDraggingOver = $state(false);
  let themeNotice = $state("");
  let updateNotice = $state("");

  // Features held off because Edge runs a build the split DLL was not made for.
  const unsupported = $derived(new Set(appState?.unsupported ?? []));
  // A held-off switch can still be turned off, never on.
  const isSplitHeldOff = $derived(unsupported.has(Feature.NativeSplit) && appState?.features.nativeSplit !== true);
  const isThemingHeldOff = $derived(unsupported.has(Feature.Theming) && appState?.features.theming !== true);
  const isHandlersHeldOff = $derived(
    unsupported.has(Feature.ProtocolHandlers) && appState?.features.protocolHandlers !== true
  );
  const isFindHeldOff = $derived(
    unsupported.has(Feature.EnhancedFind) && appState?.features.enhancedFind !== true
  );
  const isWebUiHeldOff = $derived(
    unsupported.has(Feature.WebUiFollowsTheme) && appState?.features.webUiFollowsTheme !== true
  );
  // What a locked card says about why, on the card itself.
  function heldOffNote(isHeldOff: boolean): string {
    return isHeldOff && appState ? describeHeldOff(appState) : "";
  }
  // The update notice replaces the standing hint it would only repeat.
  const notices = $derived(
    [
      updateNotice || (appState ? describeUnsupported(appState) : ""),
      themeNotice
    ].filter(notice => notice.length > 0)
  );

  onMount(async () => {
    appState = await bridge.loadState();
    const missedUpdate = await bridge.takeEdgeUpdate();
    if (missedUpdate) {
      updateNotice = describeUpdate(missedUpdate);
    }
  });

  // Edge came up on a build not seen before; features that depend on the old
  // msedge.dll may have been switched off.
  $effect(() => {
    const unlisten = listen<bridge.EdgeUpdate>(bridge.EDGE_UPDATED_EVENT, event => {
      updateNotice = describeUpdate(event.payload);
      void bridge.loadState().then(state => {
        appState = state;
      });
    });
    return () => {
      void unlisten.then(stop => {
        stop();
      });
    };
  });

  // Coming back to the panel picks up themes installed in Edge meanwhile.
  $effect(() => {
    function refresh(): void {
      void bridge.loadState().then(state => {
        appState = state;
      });
    }
    window.addEventListener("focus", refresh);
    return () => {
      window.removeEventListener("focus", refresh);
    };
  });

  // A local theme edited on disk was re-checked and re-installed (or refused).
  $effect(() => {
    const unlisten = listen<bridge.ThemeRefresh>(bridge.THEME_REFRESHED_EVENT, event => {
      themeNotice = event.payload.error ?? "";
    });
    return () => {
      void unlisten.then(stop => {
        stop();
      });
    };
  });

  $effect(() => {
    const unlisten = getCurrentWebview().onDragDropEvent(event => {
      const { type } = event.payload;
      isDraggingOver = type === "enter" || type === "over";

      if (type === "drop") {
        void addDropped(event.payload.paths);
      }
    });
    return () => {
      void unlisten.then(stop => {
        stop();
      });
    };
  });

  // Every dropped folder, .zip or .crx becomes a theme (the core tells them apart).
  async function addDropped(paths: string[]): Promise<void> {
    themeNotice = "";
    for (const path of paths) {
      try {
        appState = await bridge.addLocalTheme(path);
      } catch (error) {
        themeNotice = String(error);
      }
    }
  }

  async function onSplit(value: boolean): Promise<void> {
    appState = await bridge.setSplit(value);
  }

  async function onTheming(value: boolean): Promise<void> {
    appState = await bridge.setTheming(value);
  }

  async function onQuietDevMode(value: boolean): Promise<void> {
    appState = await bridge.setQuietDevMode(value);
  }

  async function onStartWithWindows(value: boolean): Promise<void> {
    appState = await bridge.setStartWithWindows(value);
  }

  async function onProtocolHandlers(value: boolean): Promise<void> {
    appState = await bridge.setProtocolHandlers(value);
  }

  async function onEnhancedFind(value: boolean): Promise<void> {
    appState = await bridge.setEnhancedFind(value);
  }

  async function onWebUiFollowsTheme(value: boolean): Promise<void> {
    appState = await bridge.setWebUiFollowsTheme(value);
  }

  async function onAddStore(url: string): Promise<void> {
    appState = await bridge.addStoreTheme(url);
  }

  async function onAddLocal(kind: LocalThemeKind): Promise<void> {
    const picked = await open({
      multiple: false,
      directory: kind === LocalThemeKind.Folder,
      filters: kind === LocalThemeKind.File ? [{
        name: "Edge theme",
        extensions: ["crx", "zip"]
      }] : []
    });
    if (typeof picked === "string") {
      appState = await bridge.addLocalTheme(picked);
    }
  }

  async function onRemove(id: string): Promise<void> {
    appState = await bridge.removeTheme(id);
  }

  async function onSetMode(mode: Mode, id: string): Promise<void> {
    appState = await bridge.setModeTheme(mode, id);
  }
</script>

{#if appState}
  <main class="app">
    <header class="title">
      <h1>Edge+</h1>
      {#if !appState.edgeRunning}
        <p class="hint">Open Edge to apply changes.</p>
      {/if}
      {#each notices as notice (notice)}
        <p class="hint error">{notice}</p>
      {/each}
    </header>

    <FeatureCard
      checked={appState.features.nativeSplit}
      description={splitDescription}
      disabled={isSplitHeldOff}
      icon={splitIcon}
      note={heldOffNote(isSplitHeldOff)}
      onchange={onSplit}
      title="Split into any grid"
    />

    <ThemingCard
      description={themeDescription}
      disabled={isThemingHeldOff}
      edgeThemes={appState.edgeThemes}
      enabled={appState.features.theming}
      icon={themeIcon}
      note={heldOffNote(isThemingHeldOff)}
      {onAddLocal}
      {onAddStore}
      {onRemove}
      {onSetMode}
      onToggle={onTheming}
      theming={appState.theming}
    />

    <FeatureCard
      checked={appState.features.webUiFollowsTheme}
      description={webUiDescription}
      disabled={isWebUiHeldOff}
      icon={webUiIcon}
      note={heldOffNote(isWebUiHeldOff)}
      onchange={onWebUiFollowsTheme}
      title="Settings and extensions pages follow the theme"
    />

    <FeatureCard
      checked={appState.features.enhancedFind}
      description={FIND_DESCRIPTION}
      disabled={isFindHeldOff}
      icon={findIcon}
      note={heldOffNote(isFindHeldOff)}
      onchange={onEnhancedFind}
      title={FIND_TITLE}
    />

    <FeatureCard
      checked={appState.features.quietDevMode}
      description={devModeDescription}
      icon={shieldIcon}
      onchange={onQuietDevMode}
      title="Hide the developer-mode prompt"
    />

    <FeatureCard
      checked={appState.features.protocolHandlers}
      description={handlersDescription}
      disabled={isHandlersHeldOff}
      icon={handlerIcon}
      note={heldOffNote(isHandlersHeldOff)}
      onchange={onProtocolHandlers}
      title="Ask to handle links, like Chrome"
    />

    <FeatureCard
      checked={appState.startWithWindows}
      description={startupDescription}
      icon={startupIcon}
      onchange={onStartWithWindows}
      title="Start Edge+ when Windows starts"
    />
  </main>
{/if}

<div class="drop-veil" class:visible={isDraggingOver} aria-hidden="true">
  <p>Drop a theme folder, .zip or .crx to add it</p>
</div>

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

  .drop-veil {
    position: fixed;
    inset: 0;
    display: grid;
    place-items: center;
    margin: var(--space-m);
    border: 2px dashed var(--brand);
    border-radius: var(--radius-l);
    background: color-mix(in sRGB, var(--bg-canvas) 85%, transparent);
    color: var(--fg-1);
    font-size: 16px;
    opacity: 0%;
    pointer-events: none;
    transition: opacity 120ms ease;
  }

  .drop-veil.visible {
    opacity: 100%;
  }

  .error {
    color: var(--danger-fg);
  }

  .hint {
    margin-block: var(--space-xs) 0;
    margin-inline: 0;
    color: var(--fg-3);
    font-size: 12.5px;
  }
</style>
