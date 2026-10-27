import type { ItemsComponent, ItemsComponentProps } from "./items/items-props.ts";

export type ListViewComponentProps<Item = object> = ItemsComponentProps<Item>;

declare const ListView: ItemsComponent;
export default ListView;
