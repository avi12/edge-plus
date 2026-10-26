<script lang="ts">
  import { untrack } from "svelte";
  import type { FrameworkElement, Grid } from "../generated/bindings.ts";
  import { control } from "../runtime.ts";
  import type { WindowComponentProps } from "./Window.d.svelte.ts";
  import { attachWindow } from "../../window/window-host.svelte.ts";

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

<grid rows={hasTitleBar ? "Auto,*" : "*"} {@attach control((content: Grid) => untrack(() => open(content)))}>
  {#if titleBar !== undefined}
    <grid grid.row={0} {@attach control((bar: Grid) => untrack(() => holdTitleBar(bar)))}>
      {@render titleBar()}
    </grid>
  {/if}
  <grid grid.row={hasTitleBar ? 1 : 0}>
    {@render children?.()}
  </grid>
</grid>
