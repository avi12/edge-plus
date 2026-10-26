import type { ItemsComponentProps } from "./items/items-props.ts";
import type { ComponentInternals } from "svelte";

export type ListViewComponentProps<Item = object> = ItemsComponentProps<Item>;

declare function ListView<Item>(
  internals: ComponentInternals,
  props: ListViewComponentProps<Item>
): Record<string, never>;
export default ListView;
