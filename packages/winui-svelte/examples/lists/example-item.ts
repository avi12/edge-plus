export interface ExampleItem {
  id: string;
  name: string;
}

export function makeItems(count: number): ExampleItem[] {
  return Array.from({ length: count }, (_unused, index) => ({
    id: `row-${index}`,
    name: `Item ${index}`
  }));
}
