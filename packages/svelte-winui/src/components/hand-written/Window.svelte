<script lang="ts">
  import { attachWindow } from "../../window/window-host.svelte.ts";
  import type { FrameworkElement, Grid } from "../generated/bindings.ts";
  import { control } from "../runtime.ts";
  import type { WindowComponentProps } from "./Window.d.svelte.ts";
  import { untrack } from "svelte";

  // A native window around its content. Mount it as the app's root (the
  // component itself is the window, nothing is placed in a page).
  let { handle = $bindable(), titleBar, children, ...rest }: WindowComponentProps = $props();
  let titleBarControl = $state.raw<FrameworkElement | null>(null);
  const hasTitleBar = $derived(titleBar !== undefined);

  function open(content: Grid): () => void {
    const attached = attachWindow(content, rest, () => titleBarControl);
    handle = attached.handle;
    return attached.dispose;
  }

  function holdTitleBar(bar: Grid): () => void {
    titleBarControl = bar;
    return () => {
      titleBarControl = null;
    };
  }
</script>

<grid {@attach control((content: Grid) => untrack(() => open(content)))} rows={hasTitleBar ? "Auto,*" : "*"}>
  {#if titleBar !== undefined}
    <grid {@attach control((bar: Grid) => untrack(() => holdTitleBar(bar)))} grid.row={0}>
      {@render titleBar()}
    </grid>
  {/if}
  <grid grid.row={hasTitleBar ? 1 : 0}>
    {@render children?.()}
  </grid>
</grid>
