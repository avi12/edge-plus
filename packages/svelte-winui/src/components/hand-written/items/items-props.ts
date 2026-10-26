import type { ElementProps } from "../../convert.ts";
import type { ListViewBaseProps } from "../../generated/controls/ListViewBase.svelte.ts";
import type { ItemsHostProps, ListHandle } from "./items-host.svelte.ts";

// What the list controls take besides the control's own properties: the items
// replace the children, the item template and the items source, which the
// component owns.
type OwnedByComponent = "children" | "itemsSource" | "itemTemplate" | "itemTemplateSelector";

interface HandleProps {
  // The list's imperative side (`bind:handle`).
  handle?: ListHandle;
}

export type ItemsComponentProps<Item> = ItemsHostProps<Item>
  & Omit<ListViewBaseProps, OwnedByComponent>
  & ElementProps
  & HandleProps;
