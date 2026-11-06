<script lang="ts">
  import "../style/accent-button.css";
  import EdgeThemeList from "./EdgeThemeList.svelte";
  import OwnThemeList from "./OwnThemeList.svelte";
  import ThemePicker from "./ThemePicker.svelte";
  import type { Action, ColorMode, ThemeOption, ThemingView } from "#src/protocol.g.ts";
  import { usePanel } from "#src/window/host/panel.svelte.ts";
  import {
    Button,
    Grid,
    RepositionThemeTransition,
    StackPanel,
    TextBlock,
    TextBox
  } from "winui-svelte/components";
  import { Visibility } from "winui-svelte/enums";
  import { ButtonKind, Form, formButton, textField } from "winui-svelte/forms";
  import { z } from "zod";

  interface Props {
    isOn: boolean;
    theming: ThemingView;
  }

  const { isOn, theming }: Props = $props();
  const panel = usePanel();

  const DEFAULT_THEME: ThemeOption = {
    id: "",
    name: "Default theme"
  };
  const THEME_FILE_TYPES = [".crx", ".zip"];
  const storeLinkSchema = z.object({ storeLink: z.string().trim().min(1) });

  const options = $derived([DEFAULT_THEME, ...theming.options]);
  const hasThemes = $derived(theming.own.length > 0 || theming.edge.length > 0);

  let storeLink = $state("");
  let isBusy = $state(false);
  let error = $state("");

  async function runBusy(action: Action) {
    error = "";
    isBusy = true;
    const failure = await panel.run(action);
    isBusy = false;
    error = failure ?? "";
    return failure === null;
  }

  async function addStore({ data }: { data: z.output<typeof storeLinkSchema> }) {
    const isAdded = await runBusy({
      kind: "addStoreTheme",
      url: data.storeLink
    });
    if (isAdded) {
      storeLink = "";
    }
  }

  async function addLocal(path: string | null | undefined) {
    const hasPath = path != null;
    if (hasPath) {
      await runBusy({
        kind: "addLocalTheme",
        path
      });
    }
  }

  function assign({ mode, id }: {
    mode: ColorMode;
    id: string;
  }) {
    void runBusy({
      kind: "assignTheme",
      mode,
      id
    });
  }

  async function reorder(ids: string[]) {
    const failure = await panel.run({
      kind: "reorderThemes",
      ids
    });
    const isFailure = failure !== null;
    if (isFailure) {
      error = failure;
    }
  }
</script>

<StackPanel class="theming" class:dimmed={!isOn}>
  {#snippet childrenTransitions()}
    <RepositionThemeTransition />
  {/snippet}
  <Form onsubmit={addStore} schema={storeLinkSchema}>
    <Grid class="store-add">
      <TextBox
        {@attach textField({
          name: "storeLink",
          required: true,
          label: "Theme store link"
        })}
        automationId="store-link"
        placeholderText="Paste a Chrome Web Store or Edge Add-ons link"
        bind:text={storeLink}
      />
      <Button class="accent-button store-add-button" {@attach formButton({ kind: ButtonKind.SUBMIT })} automationId="store-add" isEnabled={!isBusy}>Add</Button>
    </Grid>
  </Form>
  <StackPanel class="local-add">
    <TextBlock class="local-add-label">Or add a local theme</TextBlock>
    <Button
      isEnabled={!isBusy}
      onclick={async () => addLocal(await panel.window?.pickFile({ fileTypes: THEME_FILE_TYPES }))}
    >File…</Button>
    <Button isEnabled={!isBusy} onclick={async () => addLocal(await panel.window?.pickFolder())}>Folder…</Button>
  </StackPanel>
  <TextBlock class="error" visibility={error.length > 0 ? Visibility.Visible : Visibility.Collapsed}>{error}</TextBlock>
  <ThemePicker
    assignedId={theming.light} onpick={id => assign({
      mode: "light",
      id
    })} {options} pickerId="light">When Windows is light</ThemePicker>
  <ThemePicker
    assignedId={theming.dark} onpick={id => assign({
      mode: "dark",
      id
    })} {options} pickerId="dark">When Windows is dark</ThemePicker>
  <OwnThemeList
    onremove={id => runBusy({
      kind: "removeTheme",
      id
    })} onreorder={reorder} rows={theming.own} />
  <EdgeThemeList {hasThemes} rows={theming.edge} />
</StackPanel>

<style>
  .theming {
    gap: 8px;
  }

  .dimmed {
    opacity: 0.5;
  }

  .store-add {
    grid-template-columns: 1fr auto;
    column-gap: 8px;
  }

  .store-add-button {
    grid-column: 2;
  }

  .local-add {
    flex-direction: row;
    gap: 8px;
  }

  .local-add-label {
    align-self: center;
    color: var(--text-fill-color-secondary);
    -winui-based-on: CaptionTextBlockStyle;
  }

  .error {
    color: var(--system-fill-color-critical);
    text-wrap: wrap;
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
