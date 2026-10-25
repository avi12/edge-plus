<script lang="ts">
  import type { ThemingLabels } from "../protocol.g.ts";
  import * as motion from "./motion.ts";
  import { withControl } from "./svelte-renderer.ts";
  import type { ComboBox, FrameworkElement } from "./winui.ts";
  import {
    KeyRoutedEventArgs,
    projectAs,
    StackPanel,
    TextBox,
    VirtualKey,
    Visibility
  } from "./winui.ts";

  // The theme library under the theming switch: add from a store link or a
  // local file, pick a theme for each Windows mode, and order the list.
  interface Props {
    labels: ThemingLabels;
    isOn: boolean;
    // While an add or a choice runs, the add buttons wait for it.
    isBusy: boolean;
    error: string;
    lightPicker: ComboBox;
    darkPicker: ComboBox;
    ownThemes: FrameworkElement;
    edgeThemes: FrameworkElement;
    // Resolves to whether the theme was added.
    addStoreTheme: (link: string) => Promise<boolean>;
    pickFile: () => void;
    pickFolder: () => void;
  }

  const {
    labels,
    isOn,
    isBusy,
    error,
    lightPicker,
    darkPicker,
    ownThemes,
    edgeThemes,
    addStoreTheme,
    pickFile,
    pickFolder
  }: Props = $props();

  const fullOpacity = 1;
  const dimmedOpacity = 0.5;

  let storeLink: TextBox | null = null;

  async function addStore(): Promise<void> {
    const box = storeLink;
    if (box === null) {
      return;
    }

    const link = box.text.trim();
    if (link.length === 0) {
      return;
    }

    const isAdded = await addStoreTheme(link);
    if (isAdded) {
      box.text = "";
    }
  }

  function addStoreOnEnter(_sender: unknown, event: unknown): void {
    const isEnter = projectAs(event, KeyRoutedEventArgs).key === VirtualKey.Enter;
    if (isEnter) {
      void addStore();
    }
  }
</script>

<!-- The picker's minimum width fits the longest choice it offers. -->
{#snippet pickerRow(label: string, picker: ComboBox)}
  <grid columns="*,Auto" column-spacing="8">
    <text-block class="CardTitle" vertical-alignment="Center">{label}</text-block>
    <host element={picker} min-width="200" automation-properties.name={label} grid.column={1} />
  </grid>
{/snippet}

<stack-panel spacing="8" opacity={isOn ? fullOpacity : dimmedOpacity} {@attach withControl(StackPanel, body => motion.animate(body, motion.reposition))}>
  <grid columns="*,Auto" column-spacing="8">
    <text-box placeholder-text={labels.storeLinkPlaceholder} onkeydown={addStoreOnEnter} {@attach withControl(TextBox, box => {
      storeLink = box;
    })} />
    <button class="AccentButton" is-enabled={!isBusy} onclick={() => void addStore()} grid.column={1}>{labels.add}</button>
  </grid>
  <stack-panel orientation="Horizontal" spacing="8">
    <text-block class="CardDescription" vertical-alignment="Center">{labels.addLocal}</text-block>
    <button is-enabled={!isBusy} onclick={pickFile}>{labels.pickFile}</button>
    <button is-enabled={!isBusy} onclick={pickFolder}>{labels.pickFolder}</button>
  </stack-panel>
  <text-block class="CardError" visibility={error.length > 0 ? Visibility.Visible : Visibility.Collapsed}>{error}</text-block>
  {@render pickerRow(labels.whenLight, lightPicker)}
  {@render pickerRow(labels.whenDark, darkPicker)}
  <host element={ownThemes} />
  <host element={edgeThemes} />
</stack-panel>
