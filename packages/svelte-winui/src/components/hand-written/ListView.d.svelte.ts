import type { ComponentInternals } from "svelte";
import type { ItemsComponentProps } from "./items-props.ts";

export type ListViewComponentProps<Item = unknown> = ItemsComponentProps<Item>;

declare function ListView<Item>(
  internals: ComponentInternals,
  props: ListViewComponentProps<Item>
): Record<string, never>;
export default ListView;
