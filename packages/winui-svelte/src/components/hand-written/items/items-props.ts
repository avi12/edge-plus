import type { ElementProps } from "../../convert.ts";
import type { ListViewBaseProps } from "../../generated/controls/ListViewBase.svelte.ts";
import type { ItemsHostProps, ListHandle } from "./items-host.svelte.ts";
import type { ComponentConstructorOptions, ComponentInternals, SvelteComponent } from "svelte";

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

// A list component, generic over its items, typed as svelte2tsx types a
// generic .svelte file: svelte-check keeps the generic of a component with a
// construct signature, and of a plain function component infers its props once
// with Item unknown, so `items={rows}` would not type the item snippet's row.
export interface ItemsComponent {
  new <Item>(
    options: ComponentConstructorOptions<ItemsComponentProps<Item>>
  ): SvelteComponent<ItemsComponentProps<Item>>;
  <Item>(internals: ComponentInternals, props: ItemsComponentProps<Item>): Record<string, never>;
}
