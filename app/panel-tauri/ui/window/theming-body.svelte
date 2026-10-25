<script lang="ts">
  import type { Action, CardView, ColorMode, ThemingView } from "../protocol.g.ts";
  import EdgeThemeList from "./edge-theme-list.svelte";
  import * as motion from "./motion.ts";
  import { OwnThemeList } from "./own-theme-list.ts";
  import { usePanel } from "./panel.svelte.ts";
  import ThemePicker from "./theme-picker.svelte";
  import { LocalThemeKind } from "./theming-card.ts";
  import { KeyRoutedEventArgs, projectAs, VirtualKey } from "./winui.ts";
  import { Button, Grid, StackPanel, TextBlock, TextBox } from "svelte-winui";

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

  // The list is still drawn from TypeScript (a ListView with its own drag and
  // keyboard reordering), until the package has a list component.
  // svelte-ignore state_referenced_locally
  const ownThemes = new OwnThemeList(theming.labels, {
    remove(id) {
      void runBusy({
        kind: "removeTheme",
        id
      });
    },
    reorder(ids) {
      void runQuietly({
        kind: "reorderThemes",
        ids
      });
    },
    move(id, offset) {
      return runQuietly({
        kind: "moveTheme",
        id,
        offset
      });
    }
  });

  $effect(() => {
    ownThemes.update(theming.own);
  });

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

  // None of runBusy's busy state: holding the add buttons for a reorder
  // flickered File… and Folder… on every drop.
  async function runQuietly(action: Action): Promise<void> {
    const failure = await panel.run(action);
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

<StackPanel spacing={8} opacity={card.isOn ? fullOpacity : dimmedOpacity} childrenTransitions={motion.applied(motion.reposition)}>
  <Grid columns="*,Auto" columnSpacing={8}>
    <TextBox bind:text={storeLink} placeholderText={theming.labels.storeLinkPlaceholder} onKeyDown={addStoreOnEnter} />
    <Button class="accent-button" isEnabled={!isBusy} onClick={() => void addStore()} gridColumn={1}>{theming.labels.add}</Button>
  </Grid>
  <StackPanel orientation="Horizontal" spacing={8}>
    <TextBlock class="card-description" verticalAlignment="Center" text={theming.labels.addLocal} />
    <Button isEnabled={!isBusy} onClick={() => void addLocal(LocalThemeKind.File)}>{theming.labels.pickFile}</Button>
    <Button isEnabled={!isBusy} onClick={() => void addLocal(LocalThemeKind.Folder)}>{theming.labels.pickFolder}</Button>
  </StackPanel>
  <TextBlock class="error" visibility={error.length > 0 ? "Visible" : "Collapsed"} text={error} />
  <ThemePicker label={theming.labels.whenLight} options={theming.options} assignedId={theming.light} onPick={id => assign("light", id)} />
  <ThemePicker label={theming.labels.whenDark} options={theming.options} assignedId={theming.dark} onPick={id => assign("dark", id)} />
  <host element={ownThemes.view} />
  <EdgeThemeList rows={theming.edge} empty={theming.empty} />
</StackPanel>

<style>
  :global(.error) {
    -winui-based-on: CaptionTextBlockStyle;
    color: var(--system-fill-color-critical);
    text-wrap: wrap;
  }
</style>
