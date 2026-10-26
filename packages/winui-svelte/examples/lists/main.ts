import type { ExampleHost } from "../harness/contract.ts";
import ListsApp from "./ListsApp.svelte";
import { mountControl } from "winui-svelte";

// The lists example: a window with a ListView of rows (examples/lists/run.ps1
// opens it off screen and drives it; test/lists.test.ts checks it).
export function start(host: ExampleHost): void {
  mountControl(ListsApp, { host });
}
