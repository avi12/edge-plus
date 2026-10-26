<script generics="Item" lang="ts">
  import type { GridView } from "../generated/bindings.ts";
  import { connectGridView } from "../generated/controls/GridView.svelte.ts";
  import { control } from "../runtime.ts";
  import type { GridViewComponentProps } from "./GridView.d.svelte.ts";
  import { attachItems } from "./items-host.svelte.ts";
  import { untrack } from "svelte";

  // Written by hand (scripts/codegen/tables.ts: handWrittenComponents): the
  // generated GridView has no items, only children.
  let {
    selectedIndex = $bindable(),
    selectedItem = $bindable(),
    selectedValue = $bindable(),
    handle = $bindable(),
    class: className,
    style,
    ...rest
  }: GridViewComponentProps<Item> = $props();
  const bound = {
    get selectedIndex() {
      return selectedIndex;
    },
    set selectedIndex(value) {
      selectedIndex = value;
    },
    get selectedItem() {
      return selectedItem;
    },
    set selectedItem(value) {
      selectedItem = value;
    },
    get selectedValue() {
      return selectedValue;
    },
    set selectedValue(value) {
      selectedValue = value;
    }
  };

  function connect(native: GridView): () => void {
    const disconnectProperties = connectGridView(native, rest, bound);
    const items = attachItems(native, rest);
    handle = items.handle;
    return () => {
      items.dispose();
      disconnectProperties();
    };
  }
</script>

<grid-view {style} class={className} {@attach control((native: GridView) => untrack(() => connect(native)))} />
