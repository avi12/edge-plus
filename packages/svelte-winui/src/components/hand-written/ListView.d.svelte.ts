import type { ItemsComponentProps } from "./items-props.ts";
import type { ComponentInternals } from "svelte";

export type ListViewComponentProps<Item = unknown> = ItemsComponentProps<Item>;

declare function ListView<Item>(
  internals: ComponentInternals,
  props: ListViewComponentProps<Item>
): Record<string, never>;
export default ListView;
