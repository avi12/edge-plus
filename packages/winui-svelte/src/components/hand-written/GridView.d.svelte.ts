import type { ItemsComponent, ItemsComponentProps } from "./items/items-props.ts";

export type GridViewComponentProps<Item = object> = ItemsComponentProps<Item>;

declare const GridView: ItemsComponent;
export default GridView;
