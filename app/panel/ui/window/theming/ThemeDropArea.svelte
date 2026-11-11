<script lang="ts">
  import { THEME_FILE_TYPES } from "./theme-files.ts";
  import { usePanel } from "#src/window/host/panel.svelte.ts";
  import type { Snippet } from "svelte";
  import { StackPanel } from "winui-svelte/components";
  import { DropEffect, dropzone } from "winui-svelte/data-transfer";
  import type { DragEvent } from "winui-svelte/data-transfer";

  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
  const panel = usePanel();
  const DROP_CAPTION = "Drop a folder to add a theme";
  let isDropOver = $state(false);

  function showDropFeedback(event: DragEvent) {
    event.dataTransfer.dropEffect = DropEffect.link;
    event.setFeedback({
      caption: DROP_CAPTION,
      isGlyphVisible: true
    });
  }
</script>

<StackPanel
  class:drop-over={isDropOver}
  {@attach dropzone({
    accept: ["directory", ...THEME_FILE_TYPES],
    onDragStateChange: isOver => {
      isDropOver = isOver;
    },
    onDragEnter: showDropFeedback,
    onDragOver: showDropFeedback,
    onDrop: event => panel.addDroppedThemes(event),
    onReject: () => panel.rejectDroppedThemes()
  })}
>
  {@render children()}
</StackPanel>

<style>
  .drop-over {
    background: var(--subtle-fill-color-secondary);
  }
</style>
