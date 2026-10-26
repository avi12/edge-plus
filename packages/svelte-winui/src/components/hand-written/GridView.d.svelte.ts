import type { ItemsComponentProps } from "./items-props.ts";
import type { ComponentInternals } from "svelte";

export type GridViewComponentProps<Item = unknown> = ItemsComponentProps<Item>;

declare function GridView<Item>(
  internals: ComponentInternals,
  props: GridViewComponentProps<Item>
): Record<string, never>;
export default GridView;
