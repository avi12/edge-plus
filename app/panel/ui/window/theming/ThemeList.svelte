<script lang="ts">
  import { type KeyEventArguments, keyEventOf, type KeyHandlerSender } from "./key-arguments.ts";
  import { movedIds } from "./reorder-math.ts";
  import { ThemeReorder } from "./theme-reorder.svelte.ts";
  import { ThemeBadgeName } from "./theme-vocabulary.ts";
  import ThemeRow from "./ThemeRow.svelte";
  import type { ColorMode, ThemeRow as Row } from "#src/protocol.g.ts";
  import CardDescription from "#src/window/cards/CardDescription.svelte";
  import { CssVisibility } from "#src/window/style/css-visibility.ts";
  import { untrack } from "svelte";
  import { flip } from "svelte/animate";
  import { SvelteSet } from "svelte/reactivity";
  import { slide } from "svelte/transition";
  import { StackPanel } from "winui-svelte/components";
  import { VirtualKey } from "winui-svelte/enums";
  import { modifierKeysHeld } from "winui-svelte/input";

  interface Props {
    rows: Row[];
    /** Edge's default theme, the fixed first row; its id is empty. */
    defaultTheme: Row;
    /** A theme is being added to be used at once (Shift): the row that appears for it waits for Edge to wear it. */
    isUsingNewTheme: boolean;
    onremove: (id: string) => void;
    onremoveedge: (id: string) => void;
    onreorder: (ids: string[]) => void;
    onassign: (choice: {
      mode: ColorMode;
      id: string;
    }) => void;
  }

  const { rows, defaultTheme, isUsingNewTheme, onremove, onremoveedge, onreorder, onassign }: Props = $props();
  const MIN_REORDERABLE_THEMES = 2;
  /** WinUI's fast motion duration. */
  const EMPTY_LINE_MILLISECONDS = 167;
  /** The glide of a row to its new place when a key moved it. */
  const KEY_MOVE_MILLISECONDS = 200;
  const canReorder = $derived(rows.length >= MIN_REORDERABLE_THEMES);
  const hasThemes = $derived(rows.length > 0);
  // The order the user just made, shown until the core's view agrees: the
  // rows stand in it at once instead of snapping back for the round trip.
  let madeOrder = $state<string[] | null>(null);
  const shown = $derived.by(() => {
    if (madeOrder === null) {
      return rows;
    }

    const byId = new Map(rows.map(row => [row.id, row]));
    const inMadeOrder = madeOrder.flatMap(id => byId.get(id) ?? []);
    const added = rows.filter(row => !madeOrder?.includes(row.id));
    return [...inMadeOrder, ...added];
  });

  $effect(() => {
    const isSameSize = madeOrder?.length === rows.length;
    const isAgreed = isSameSize && madeOrder?.every((id, iRow) => rows[iRow]?.id === id);
    if (isAgreed) {
      madeOrder = null;
    }
  });

  function commit(ids: string[]) {
    madeOrder = ids;
    onreorder(ids);
  }

  // The rows that were there before this render: one that is not among them has just been added.
  const knownIds = new SvelteSet(untrack(() => rows.map(row => row.id)));
  $effect(() => {
    knownIds.clear();
    for (const row of shown) {
      knownIds.add(row.id);
    }
  });

  const reorder = new ThemeReorder({
    orderOf: () => shown.map(row => row.id),
    onCommit: commit
  });

  /** How far Alt with each arrow key moves a row. */
  const KEY_STEPS = new Map<number, number>([
    [VirtualKey.Up, -1],
    [VirtualKey.Down, 1]
  ]);

  function moveByKey({ args, id }: {
    args: KeyEventArguments;
    id: string;
  }) {
    const keyEvent = keyEventOf(args);
    const step = KEY_STEPS.get(keyEvent.key);
    const isMoveKey = step !== undefined && modifierKeysHeld().altKey;
    if (!isMoveKey) {
      return;
    }

    keyEvent.handled = true;
    const ids = shown.map(row => row.id);
    const index = ids.indexOf(id);
    const target = index + step;
    const isInside = target >= 0 && target < ids.length;
    if (isInside) {
      commit(
        movedIds({
          ids,
          index,
          target
        })
      );
    }
  }

  function isEdgeOnly(row: Row) {
    const isPanelOwned = row.badges.includes(ThemeBadgeName.local) || row.badges.includes(ThemeBadgeName.store);
    return !isPanelOwned && row.badges.includes(ThemeBadgeName.installedInEdge);
  }

  function glideMilliseconds() {
    if (reorder.isCommitting) {
      return 0;
    }

    return KEY_MOVE_MILLISECONDS;
  }
</script>

<StackPanel>
  <ThemeRow
    {canReorder}
    isFixed
    onassign={mode => onassign({
      mode,
      id: defaultTheme.id
    })}
    onremove={null}
    theme={defaultTheme}
  />
  <StackPanel
    style:visibility={hasThemes ? CssVisibility.visible : CssVisibility.hidden}
    class="themes"
    automationHelpText="Drag a theme, or press Alt+Up or Alt+Down, to move it in this list."
    automationName="Themes"
  >
    {#each shown as row (row.id)}
      {@const isEdge = isEdgeOnly(row)}
      {@const handlers = canReorder ? reorder.handlers(row.id) : undefined}
      <border
        class="theme-list-item"
        class:lifted={reorder.draggedId === row.id}
        {@attach reorder.row(row.id)}
        onpointercanceled={handlers?.onpointercanceled}
        onpointermoved={handlers?.onpointermoved}
        onpointerpressed={handlers?.onpointerpressed}
        onpointerreleased={handlers?.onpointerreleased}
        onpreviewkeydown={(_sender: KeyHandlerSender, args: KeyEventArguments) => moveByKey({
          args,
          id: row.id
        })}
        animate:flip={{ duration: glideMilliseconds }}
      >
        <ThemeRow
          {canReorder}
          isNew={!knownIds.has(row.id)}
          isUsedNow={isUsingNewTheme}
          onassign={mode => onassign({
            mode,
            id: row.id
          })}
          onremove={() => (isEdge ? onremoveedge(row.id) : onremove(row.id))}
          shouldConfirmRemove={isEdge}
          theme={row}
        />
      </border>
    {/each}
  </StackPanel>
  <!-- Outside the rows' panel: WinUI holds an added child back until its deletes and moves
       finish, so the line showed up late; Svelte's slide starts at once. -->
  {#if !hasThemes}
    <border transition:slide={{ duration: EMPTY_LINE_MILLISECONDS }}>
      <CardDescription>No themes yet - paste a store link or add a local theme (.crx, .zip or folder).</CardDescription>
    </border>
  {/if}
</StackPanel>

<style>
  .theme-list-item {
    min-height: 36px;
    padding: 0;
    background: transparent;
  }

  /* Opaque and above the others while it is dragged: the rows it passes slide under it. */
  .lifted {
    z-index: 1;
    background: var(--card-background-fill-color-default);
    -winui-elevation: 24px;
  }
</style>
