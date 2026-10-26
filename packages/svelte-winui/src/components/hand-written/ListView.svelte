<script generics="Item" lang="ts">
  import type { ListView } from "../generated/bindings.ts";
  import { connectListView } from "../generated/controls/ListView.svelte.ts";
  import { control } from "../runtime.ts";
  import { attachItems } from "./items/items-host.svelte.ts";
  import type { ListViewComponentProps } from "./ListView.d.svelte.ts";
  import { untrack } from "svelte";

  // Written by hand (scripts/codegen/tables.ts: handWrittenComponents): the
  // generated ListView has no items, only children.
  let {
    selectedIndex = $bindable(),
    selectedItem = $bindable(),
    selectedValue = $bindable(),
    handle = $bindable(),
    class: className,
    style,
    ...rest
  }: ListViewComponentProps<Item> = $props();
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

  function connect(native: ListView): () => void {
    const disconnectProperties = connectListView(native, rest, bound);
    const items = attachItems(native, rest);
    handle = items.handle;
    return () => {
      items.dispose();
      disconnectProperties();
    };
  }
</script>

<list-view {style} class={className} {@attach control((native: ListView) => untrack(() => connect(native)))} />
