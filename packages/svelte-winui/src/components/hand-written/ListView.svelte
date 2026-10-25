<script lang="ts" generics="Item">
  import { untrack } from "svelte";
  import type { ListView } from "../generated/bindings.ts";
  import { control } from "../runtime.ts";
  import { connectListView } from "../generated/controls/ListView.svelte.ts";
  import type { ListViewComponentProps } from "./ListView.d.svelte.ts";
  import { attachItems } from "./items-host.svelte.ts";

  // Written by hand (scripts/codegen/tables.ts: handWrittenComponents): the
  // generated ListView has no items, only children.
  let { selectedIndex = $bindable(), selectedItem = $bindable(), selectedValue = $bindable(), handle = $bindable(), class: className, style, ...rest }: ListViewComponentProps<Item> = $props();
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

<list-view class={className} {style} {@attach control((native: ListView) => untrack(() => connect(native)))} />
