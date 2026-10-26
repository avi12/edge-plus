<script lang="ts">
  import type { Action, CardView, ColorMode, ThemingView } from "../../protocol.g.ts";
  import { LocalThemeKind } from "../cards/theming-card.ts";
  import { usePanel } from "../host/panel.svelte.ts";
  import * as motion from "../style/motion.ts";
  import { KeyRoutedEventArgs, projectAs, VirtualKey } from "../winui.ts";
  import EdgeThemeList from "./EdgeThemeList.svelte";
  import OwnThemeList from "./OwnThemeList.svelte";
  import ThemePicker from "./ThemePicker.svelte";
  import {
    Button,
    Grid,
    StackPanel,
    TextBlock,
    TextBox
  } from "svelte-winui";

  // The theme library under the theming switch: add from a store link or a
  // local file, pick a theme for each Windows mode, and order the list.
  interface Props {
    card: CardView;
    theming: ThemingView;
  }

  const { card, theming }: Props = $props();
  const panel = usePanel();

  const fullOpacity = 1;
  const dimmedOpacity = 0.5;

  let storeLink = $state("");
  // While an add or a choice runs, the add buttons wait for it.
  let isBusy = $state(false);
  // Why the last add or choice failed, under the add rows.
  let error = $state("");

  async function addStore(): Promise<void> {
    const link = storeLink.trim();
    if (link.length === 0) {
      return;
    }

    const isAdded = await runBusy({
      kind: "addStoreTheme",
      url: link
    });
    if (isAdded) {
      storeLink = "";
    }
  }

  async function addLocal(kind: LocalThemeKind): Promise<void> {
    const path = await panel.pick(kind);
    if (path !== null) {
      await runBusy({
        kind: "addLocalTheme",
        path
      });
    }
  }

  function assign(mode: ColorMode, id: string): void {
    void runBusy({
      kind: "assignTheme",
      mode,
      id
    });
  }

  function addStoreOnEnter(_sender: unknown, event: unknown): void {
    const isEnter = projectAs(event, KeyRoutedEventArgs).key === VirtualKey.Enter;
    if (isEnter) {
      void addStore();
    }
  }

  function remove(id: string): void {
    void runBusy({
      kind: "removeTheme",
      id
    });
  }

  // None of runBusy's busy state: holding the add buttons for a reorder
  // flickered File… and Folder… on every drop.
  async function reorder(ids: string[]): Promise<void> {
    const failure = await panel.run({
      kind: "reorderThemes",
      ids
    });
    if (failure !== null) {
      error = failure;
    }
  }

  // Resolves to whether the core did what the action asked.
  async function runBusy(action: Action): Promise<boolean> {
    error = "";
    isBusy = true;
    const failure = await panel.run(action);
    isBusy = false;
    error = failure ?? "";
    return failure === null;
  }
</script>

<StackPanel
  childrenTransitions={motion.applied(motion.reposition)}
  opacity={card.isOn ? fullOpacity : dimmedOpacity}
  spacing={8}
>
  <Grid columnSpacing={8} columns="*,Auto">
    <TextBox
      onKeyDown={addStoreOnEnter}
      placeholderText={theming.labels.storeLinkPlaceholder}
      bind:text={storeLink}
    />
    <Button
      class="accent-button"
      gridColumn={1}
      isEnabled={!isBusy}
      onClick={() => void addStore()}
    >{theming.labels.add}</Button>
  </Grid>
  <StackPanel orientation="Horizontal" spacing={8}>
    <TextBlock class="card-description" text={theming.labels.addLocal} verticalAlignment="Center" />
    <Button isEnabled={!isBusy} onClick={() => void addLocal(LocalThemeKind.File)}>{theming.labels.pickFile}</Button>
    <Button
      isEnabled={!isBusy}
      onClick={() => void addLocal(LocalThemeKind.Folder)}
    >{theming.labels.pickFolder}</Button>
  </StackPanel>
  <TextBlock class="error" text={error} visibility={error.length > 0 ? "Visible" : "Collapsed"} />
  <ThemePicker assignedId={theming.light} label={theming.labels.whenLight} onPick={id => assign("light", id)} options={theming.options} />
  <ThemePicker assignedId={theming.dark} label={theming.labels.whenDark} onPick={id => assign("dark", id)} options={theming.options} />
  <OwnThemeList labels={theming.labels} onRemove={remove} onReorder={ids => void reorder(ids)} rows={theming.own} />
  <EdgeThemeList empty={theming.empty} rows={theming.edge} />
</StackPanel>

<style>
  :global(.error) {
    color: var(--system-fill-color-critical);
    text-wrap: wrap;
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
