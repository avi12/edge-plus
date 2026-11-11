<script lang="ts">
  import "../style/accent-button.css";
  import { type KeyEventArguments, keyEventOf, type KeyHandlerSender } from "./key-arguments.ts";
  import { THEME_FILE_TYPES } from "./theme-files.ts";
  import ThemeList from "./ThemeList.svelte";
  import type { Action, ColorMode, ThemingView } from "#src/protocol.g.ts";
  import { usePanel } from "#src/window/host/panel.svelte.ts";
  import { fade } from "svelte/transition";
  import {
    Button,
    Grid,
    RepositionThemeTransition,
    StackPanel,
    TextBlock,
    TextBox
  } from "winui-svelte/components";
  import { VirtualKey } from "winui-svelte/enums";
  import {
    ButtonKind,
    Form,
    formButton,
    type FormHandle,
    textField
  } from "winui-svelte/forms";
  import { modifierKeysHeld } from "winui-svelte/input";
  import { z } from "zod";

  interface Props {
    isOn: boolean;
    theming: ThemingView;
  }

  const { isOn, theming }: Props = $props();
  const panel = usePanel();

  /** WinUI's normal motion duration. */
  const ERROR_FADE_MILLISECONDS = 250;
  const storeLinkSchema = z.object({ storeLink: z.string().trim() });

  let storeLink = $state("");
  let storeForm = $state<FormHandle>();
  let isBusy = $state(false);

  // The box's own clear button, or deleting the text, takes the refusal of that link with it.
  $effect(() => {
    const isCleared = storeLink.length === 0;
    if (isCleared) {
      panel.themeError = "";
    }
  });

  async function runAction(action: Action) {
    const failure = await panel.run(action);
    panel.themeError = failure ?? "";
    return failure === null;
  }

  async function runBusy(action: Action) {
    isBusy = true;
    const isDone = await runAction(action);
    isBusy = false;
    return isDone;
  }

  // Shift is read when the click or the submit happens: it may be let go while a picker is open.
  async function addStore({ data }: { data: z.output<typeof storeLinkSchema> }) {
    const isNothingToAdd = data.storeLink.length === 0 || isBusy;
    if (isNothingToAdd) {
      return;
    }

    const { shiftKey: shouldUseNow } = modifierKeysHeld();
    panel.expectUsedTheme(shouldUseNow);
    const isAdded = await runBusy({
      kind: "addStoreTheme",
      url: data.storeLink,
      shouldUseNow
    });
    if (isAdded) {
      storeForm?.reset();
    }
  }

  // Enter, with or without Shift, adds the link: a TextBox may mark the key handled before the form hears it.
  function submitOnEnter(_sender: KeyHandlerSender, args: KeyEventArguments) {
    const keyEvent = keyEventOf(args);
    const isEnter = keyEvent.key === VirtualKey.Enter;
    const shouldIgnore = !isEnter || isBusy;
    if (shouldIgnore) {
      return;
    }

    keyEvent.handled = true;
    void storeForm?.requestSubmit({});
  }

  async function addLocal(pick: () => Promise<string | null | undefined> | undefined) {
    const { shiftKey: shouldUseNow } = modifierKeysHeld();
    const path = await pick();
    const hasPath = path != null;
    if (hasPath) {
      panel.expectUsedTheme(shouldUseNow);
      await runBusy({
        kind: "addLocalTheme",
        path,
        shouldUseNow
      });
    }
  }

  function assign({ mode, id }: {
    mode: ColorMode;
    id: string;
  }) {
    void runAction({
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
      panel.themeError = failure;
    }
  }
</script>

<StackPanel
  class="theming"
  class:dimmed={!isOn}
>
  {#snippet childrenTransitions()}
    <RepositionThemeTransition />
  {/snippet}
  <Form onsubmit={addStore} schema={storeLinkSchema} bind:handle={storeForm}>
    <Grid class="store-add">
      <TextBox
        {@attach textField({
          name: "storeLink",
          label: "Theme store link"
        })}
        automationId="store-link"
        onpreviewkeydown={submitOnEnter}
        placeholderText="Paste a Chrome Web Store or Edge Add-ons link"
        bind:text={storeLink}
      />
      <Button
        class="accent-button store-add-button"
        {@attach formButton({ kind: ButtonKind.SUBMIT })}
        automationId="store-add"
        isEnabled={!isBusy}
      >Add</Button>
    </Grid>
  </Form>
  <StackPanel class="local-add">
    <TextBlock class="local-add-label">Or add a local theme</TextBlock>
    <Button
      isEnabled={!isBusy}
      onclick={() => addLocal(() => panel.window?.pickFile({ fileTypes: THEME_FILE_TYPES }))}
    >File…</Button>
    <Button isEnabled={!isBusy} onclick={() => addLocal(() => panel.window?.pickFolder())}>Folder…</Button>
  </StackPanel>
  <TextBlock class="drop-hint">
    Or drag a folder, .zip or .crx from Explorer onto this card. Hold Shift while adding to use the theme right away.
  </TextBlock>
  <!-- Added and removed, not shown and hidden: a collapsed line that opens is a move for the panel's reposition
       transition, which drew it flying in from the top of the card. -->
  {#if panel.themeError.length > 0}
    <border transition:fade={{ duration: ERROR_FADE_MILLISECONDS }}>
      <TextBlock class="error">{panel.themeError}</TextBlock>
    </border>
  {/if}
  <ThemeList
    defaultTheme={theming.defaultTheme}
    isUsingNewTheme={panel.isUsingNewTheme}
    onassign={assign}
    onremove={id => runAction({
      kind: "removeTheme",
      id
    })}
    onremoveedge={id => runAction({
      kind: "removeEdgeTheme",
      id
    })}
    onreorder={reorder}
    rows={theming.themes}
  />
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

  .drop-hint {
    color: var(--text-fill-color-secondary);
    -winui-based-on: CaptionTextBlockStyle;
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
