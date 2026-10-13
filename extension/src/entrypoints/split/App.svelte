<script lang="ts">
  import splitDownIcon from "@/assets/split-down.svg?raw";
  import splitRightIcon from "@/assets/split-right.svg?raw";
  import { allowFramesInSplitTabs } from "@/features/split-panes/frame-rule";
  import type { Divider, Pane } from "@/features/split-panes/layout";
  import { Axis, dividers, layout, moveDivider, panes, remove, setUrl, split } from "@/features/split-panes/layout";
  import type { SplitState } from "@/features/split-panes/split-state";
  import { decodeState, encodeState, newPane, PANE_START_PAGE } from "@/features/split-panes/split-state";
  import { addressFrom, faviconOf } from "@/lib/address";
  import { untrack } from "svelte";
  import type { PaneCommandMessage } from "@/lib/pane-bridge";
  import { PaneCommand, PaneMessageType, paneReportSchema } from "@/lib/pane-bridge";

  /** What a pane's page may do: everything but take over the whole tab, which would end the split */
  const SANDBOX = "allow-scripts allow-same-origin allow-forms allow-popups allow-popups-to-escape-sandbox allow-modals allow-downloads allow-presentation allow-storage-access-by-user-activation";
  const ALLOW = "fullscreen; clipboard-read; clipboard-write; autoplay; encrypted-media; picture-in-picture; microphone; camera; geolocation; display-capture";
  const NEW_TAB_URL = "edge://newtab/";

  let splitState = $state<SplitState | null>(decodeState(location.hash));
  let isFrameRuleReady = $state(false);
  let draggingDivider = $state<Divider | null>(null);
  const titleByPane = $state<Record<string, string>>({});
  const frameByPane = new Map<string, HTMLIFrameElement>();
  let elSplit = $state<HTMLElement>();

  const rects = $derived(splitState ? layout({ root: splitState.root }) : new Map());
  // Sorted by id, a stable order no restructuring changes, because moving an iframe in the DOM reloads it
  const orderedPanes = $derived(splitState ? panes(splitState.root).toSorted((a, b) => a.id.localeCompare(b.id)) : []);
  const paneById = $derived(new Map(orderedPanes.map(pane => [pane.id, pane])));
  // Plain strings, so a pane whose address changes is the same item and its frame is left alone
  const paneIds = $derived(orderedPanes.map(pane => pane.id));
  const dividerList = $derived(splitState ? dividers({ root: splitState.root }) : []);
  const activePane = $derived(orderedPanes.find(pane => pane.id === splitState?.activePaneId));

  // A pane's frame loads only once the header rule is in place, or a site that refuses frames
  // would show its refusal instead
  void (async () => {
    const tab = await browser.tabs.getCurrent();
    await allowFramesInSplitTabs(tab?.id);
    isFrameRuleReady = true;
  })();

  function commit(next: SplitState) {
    splitState = next;
    history.replaceState(null, "", encodeState(next));
  }

  /** One pane left is no split: the tab simply becomes that page again */
  function commitOrUnsplit(next: SplitState) {
    const remaining = panes(next.root);
    if (remaining.length > 1) {
      commit(next);
      return;
    }
    location.replace(remaining[0]?.url || NEW_TAB_URL);
  }

  function focusPane(paneId: string) {
    const frame = frameByPane.get(paneId);
    frame?.focus();
    frame?.contentWindow?.focus();
  }

  function activate(paneId: string) {
    if (splitState && splitState.activePaneId !== paneId) {
      commit({ ...splitState, activePaneId: paneId });
    }
  }

  function navigate({ pane, text }: { pane: Pane; text: string }) {
    const url = addressFrom(text);
    const frame = frameByPane.get(pane.id);
    if (frame) {
      frame.src = url;
    }
    commit({ ...splitState!, root: setUrl({ root: splitState!.root, paneId: pane.id, url }) });
  }

  function sendCommand({ pane, command }: { pane: Pane; command: PaneCommand }) {
    const message: PaneCommandMessage = { type: PaneMessageType.command, command };
    frameByPane.get(pane.id)?.contentWindow?.postMessage(message, "*");
  }

  function splitPane({ pane, axis }: { pane: Pane; axis: Axis }) {
    const added = newPane();
    commit({ root: split({ root: splitState!.root, targetId: pane.id, newPane: added, axis }), activePaneId: added.id });
  }

  function closePane(pane: Pane) {
    const root = remove({ root: splitState!.root, paneId: pane.id })!;
    commitOrUnsplit({ root, activePaneId: panes(root)[0]!.id });
  }

  /** The frame's address is set once, here; later navigation happens inside it, and reporting it back must not reload it */
  function frameOf(paneId: string) {
    return (frame: HTMLIFrameElement) => {
      frameByPane.set(paneId, frame);
      const url = untrack(() => paneById.get(paneId)?.url);
      frame.src = url || browser.runtime.getURL(PANE_START_PAGE);
      return () => frameByPane.delete(paneId);
    };
  }

  function startDrag(e: PointerEvent, divider: Divider) {
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    draggingDivider = divider;
  }

  function drag(e: PointerEvent) {
    if (!draggingDivider || !elSplit || !splitState) {
      return;
    }
    const box = elSplit.getBoundingClientRect();
    const { within } = draggingDivider;
    const isRow = draggingDivider.axis === Axis.row;
    const fraction = isRow
      ? ((e.clientX - box.left) / box.width * 100 - within.left) / within.width
      : ((e.clientY - box.top) / box.height * 100 - within.top) / within.height;
    splitState = { ...splitState, root: moveDivider({ root: splitState.root, divider: draggingDivider, fraction }) };
  }

  function endDrag() {
    if (draggingDivider && splitState) {
      commit(splitState);
    }
    draggingDivider = null;
  }

  // The background edits the address to move focus or reshape the split; same-document, so no reload
  addEventListener("hashchange", () => {
    const next = decodeState(location.hash);
    if (!next) {
      return;
    }
    const isFocusMove = next.activePaneId !== splitState?.activePaneId;
    splitState = next;
    if (isFocusMove) {
      focusPane(next.activePaneId);
    }
  });

  addEventListener("message", e => {
    const parsed = paneReportSchema.safeParse(e.data);
    const pane = orderedPanes.find(candidate => frameByPane.get(candidate.id)?.contentWindow === e.source);
    if (!parsed.success || !pane || !splitState) {
      return;
    }
    titleByPane[pane.id] = parsed.data.title;
    if (pane.url !== parsed.data.url) {
      commit({ ...splitState, root: setUrl({ root: splitState.root, paneId: pane.id, url: parsed.data.url }) });
    }
  });

  // Clicking into a pane's page takes focus out of this document; which frame got it says which pane
  addEventListener("blur", () => {
    queueMicrotask(() => {
      const focused = orderedPanes.find(pane => frameByPane.get(pane.id) === document.activeElement);
      if (focused) {
        activate(focused.id);
      }
    });
  });

  $effect(() => {
    const title = activePane ? titleByPane[activePane.id] || activePane.url || "New pane" : "Split";
    document.title = `${title} · ${orderedPanes.length} panes`;
    const elIcon = document.querySelector<HTMLLinkElement>("link[rel=icon]");
    if (elIcon && activePane?.url) {
      elIcon.href = faviconOf(activePane.url);
    }
  });
</script>

{#if splitState}
  <main
    bind:this={elSplit}
    class="split"
    data-dragging={draggingDivider !== null}
    onpointermove={drag}
    onpointerup={endDrag}
  >
    {#each paneIds as paneId (paneId)}
      {@const pane = paneById.get(paneId)!}
      {@const rect = rects.get(paneId)}
      <section
        style:--left="{rect.left}%"
        style:--top="{rect.top}%"
        style:--width="{rect.width}%"
        style:--height="{rect.height}%"
        class="pane"
        aria-label={titleByPane[paneId] || pane.url || "New pane"}
        data-active={pane.id === splitState.activePaneId}
        role="group"
        onpointerdown={() => activate(pane.id)}
      >
        <header class="bar">
          <button aria-label="Back" data-optional="history" onclick={() => sendCommand({ pane, command: PaneCommand.back })}>&#xE72B;</button>
          <button aria-label="Forward" data-optional="history" onclick={() => sendCommand({ pane, command: PaneCommand.forward })}>&#xE72A;</button>
          <button aria-label="Reload" data-optional="history" onclick={() => sendCommand({ pane, command: PaneCommand.reload })}>&#xE72C;</button>
          <form onsubmit={e => {
            e.preventDefault();
            const elInput = (e.currentTarget as HTMLFormElement).elements.namedItem("address") as HTMLInputElement;
            navigate({ pane, text: elInput.value });
          }}>
            {#if pane.url}
              <img alt="" src={faviconOf(pane.url)} />
            {/if}
            <input name="address" aria-label="Address" placeholder="Search or enter an address" spellcheck="false" value={pane.url} />
          </form>
          <button aria-label="Split right" class="icon" data-optional="split" onclick={() => splitPane({ pane, axis: Axis.row })}>{@html splitRightIcon}</button>
          <button aria-label="Split down" class="icon" data-optional="split" onclick={() => splitPane({ pane, axis: Axis.column })}>{@html splitDownIcon}</button>
          <button aria-label="Close pane" onclick={() => closePane(pane)}>&#xE711;</button>
        </header>
        {#if isFrameRuleReady}
          <iframe {@attach frameOf(paneId)} allow={ALLOW} sandbox={SANDBOX} title={titleByPane[pane.id] || "Pane"}></iframe>
        {/if}
      </section>
    {/each}

    {#each dividerList as divider, i (i)}
      <div
        style:--left="{divider.at.left}%"
        style:--top="{divider.at.top}%"
        style:--width="{divider.at.width}%"
        style:--height="{divider.at.height}%"
        class="divider"
        aria-orientation={divider.axis === Axis.row ? "vertical" : "horizontal"}
        aria-label="Resize panes"
        role="separator"
        onpointerdown={e => startDrag(e, divider)}
      ></div>
    {/each}
  </main>
{:else}
  <p class="broken">This split's layout could not be read. Its address may have been cut short</p>
{/if}
