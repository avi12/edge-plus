<script lang="ts">
  import splitDownIcon from "@/assets/split-down.svg?raw";
  import splitRightIcon from "@/assets/split-right.svg?raw";
  import { FEATURES, FeatureId } from "@/features/catalog";
  import { fromTemplate, layout, slotCount } from "@/features/split-panes/layout";
  import type { Template } from "@/features/split-panes/layout";
  import { PRESETS } from "@/features/split-panes/presets";
  import { Command } from "@/lib/feature";
  import { MessageType, sendMessage } from "@/lib/messaging";

  /** The preset icons are drawn by the real layout code, at this size */
  const ICON_REGION = { left: 0, top: 0, width: 48, height: 32 };
  const ICON_GAP = 2;

  const splitFeature = FEATURES.find(feature => feature.id === FeatureId.splitPanes)!;

  const loading = (async () => {
    const { id: windowId } = await browser.windows.getCurrent();
    const [description, commands] = await Promise.all([
      sendMessage(MessageType.describeSplit, windowId!),
      browser.commands.getAll()
    ]);
    const shortcutByCommand = new Map(commands.map(command => [command.name, command.shortcut]));
    return { windowId: windowId!, ...description, shortcutByCommand };
  })();

  /** Each tile gives up half the gap on every side, so neighbours sit a full gap apart */
  function iconTiles(template: Template) {
    const slotPanes = Array.from({ length: slotCount(template) }, (_, slot) => ({ id: String(slot), url: "" }));
    const rects = layout({ root: fromTemplate({ template, panes: slotPanes }), region: ICON_REGION }).values();
    return [...rects].map(rect => ({
      left: rect.left + ICON_GAP / 2,
      top: rect.top + ICON_GAP / 2,
      width: rect.width - ICON_GAP,
      height: rect.height - ICON_GAP
    }));
  }

  /** Closed first: the new panes take focus, and an open popup would sit on top of them */
  function run(send: () => Promise<unknown>) {
    void send();
    close();
  }
</script>

{#await loading then state}
  <header>
    <h1>{splitFeature.title}</h1>
    {#if state.isEnabled}
      {@const isSinglePane = state.panes === 1}
      <span class="meta">{isSinglePane ? "1 pane" : `${state.panes} panes`}</span>
    {/if}
  </header>

  {#if !state.isEnabled}
    <p class="meta">Turned off in the Fix Edge app</p>
  {:else}
    {@const hasSeveralSelectedTabs = state.selectedTabs > 1}
    {@const canTile = state.panes > 2}
    {@const isSplit = state.panes > 1}

    {#snippet shortcut(command: Command)}
      {@const assignedShortcut = state.shortcutByCommand.get(command)}
      {#if assignedShortcut}
        <kbd>{assignedShortcut}</kbd>
      {/if}
    {/snippet}

    <section>
      <h2>Add a pane</h2>
      <div class="actions">
        <button onclick={() => run(() => sendMessage(MessageType.splitRight, state.windowId))}>
          {@html splitRightIcon}
          Split right
          {@render shortcut(Command.splitRight)}
        </button>
        <button onclick={() => run(() => sendMessage(MessageType.splitDown, state.windowId))}>
          {@html splitDownIcon}
          Split down
          {@render shortcut(Command.splitDown)}
        </button>
      </div>
    </section>

    <section>
      <h2>Add tabs</h2>
      {#if hasSeveralSelectedTabs}
        <button class="wide" onclick={() => run(() => sendMessage(MessageType.addSelectedTabs, state.windowId))}>
          Put the {state.selectedTabs} selected tabs in panes
          {@render shortcut(Command.addSelectedTabs)}
        </button>
      {:else}
        <p class="meta">Ctrl + click tabs in the tab strip to pick as many as you like, then add them here</p>
      {/if}
    </section>

    <section>
      <h2>Arrange this tab</h2>
      <p class="meta">The tabs to its right fill the other panes</p>
      <div class="presets">
        {#each PRESETS as preset (preset.id)}
          <button
            aria-label={preset.title}
            onclick={() => run(() => sendMessage(MessageType.applyPreset, { windowId: state.windowId, presetId: preset.id }))}
          >
            <svg aria-hidden="true" viewBox="0 0 {ICON_REGION.width} {ICON_REGION.height}">
              {#each iconTiles(preset.template) as tile, i (i)}
                <rect height={tile.height} rx="1.5" width={tile.width} x={tile.left} y={tile.top} />
              {/each}
            </svg>
            {preset.title}
          </button>
        {/each}
      </div>
    </section>

    {#if canTile}
      <button class="wide tile" onclick={() => run(() => sendMessage(MessageType.tile, state.windowId))}>
        Even out into a grid
        {@render shortcut(Command.tile)}
      </button>
    {/if}

    {#if isSplit}
      <button class="wide merge" onclick={() => run(() => sendMessage(MessageType.merge, state.windowId))}>
        Merge back into one window
        {@render shortcut(Command.unsplit)}
      </button>
    {/if}

    <footer class="meta">
      Alt + Shift + an arrow key moves between panes
    </footer>
  {/if}
{/await}
