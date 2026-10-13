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

  function iconTiles(template: Template) {
    const slots = Array.from({ length: slotCount(template) }, (_, slot) => slot);
    return [...layout({ root: fromTemplate({ template, windowIds: slots }), region: ICON_REGION, gap: ICON_GAP }).values()];
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
      <span class="meta">{state.panes === 1 ? "1 pane" : `${state.panes} panes`}</span>
    {/if}
  </header>

  {#if !state.isEnabled}
    <p class="meta">Turned off in the Fix Edge app</p>
  {:else}
    {#snippet shortcut(command: Command)}
      {#if state.shortcutByCommand.get(command)}
        <kbd>{state.shortcutByCommand.get(command)}</kbd>
      {/if}
    {/snippet}

    <section>
      <h2>Add a pane</h2>
      <div class="actions">
        <button disabled={!state.canSplitRight} onclick={() => state.canSplitRight && run(() => sendMessage(MessageType.splitRight, state.windowId))}>
          {@html splitRightIcon}
          Split right
          {@render shortcut(Command.splitRight)}
        </button>
        <button disabled={!state.canSplitDown} onclick={() => state.canSplitDown && run(() => sendMessage(MessageType.splitDown, state.windowId))}>
          {@html splitDownIcon}
          Split down
          {@render shortcut(Command.splitDown)}
        </button>
      </div>
    </section>

    <section>
      <h2>Arrange this tab</h2>
      <p class="meta">The tabs to its right fill the other panes</p>
      <div class="presets">
        {#each PRESETS as preset (preset.id)}
          {@const isFitting = state.fittingPresetIds.includes(preset.id)}
          <button
            aria-label={preset.title}
            disabled={!isFitting}
            onclick={() => isFitting && run(() => sendMessage(MessageType.applyPreset, { windowId: state.windowId, presetId: preset.id }))}
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

    {#if !state.canSplitRight || state.fittingPresetIds.length < PRESETS.length}
      <p class="meta note">Greyed out ones need a wider window - Edge won't make a pane narrower than about 500px</p>
    {/if}

    {#if state.panes > 1}
      <button class="merge" onclick={() => run(() => sendMessage(MessageType.merge, state.windowId))}>
        Merge back into one window
        {@render shortcut(Command.unsplit)}
      </button>
    {/if}

    <footer class="meta">
      Alt + Shift + an arrow key moves between panes
    </footer>
  {/if}
{/await}
