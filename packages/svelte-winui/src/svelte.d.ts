// A .svelte component, as TypeScript sees it from a .ts file.
declare module "*.svelte" {
  import type { Component } from "svelte";

  const component: Component<Record<string, unknown>>;
  export default component;
}
