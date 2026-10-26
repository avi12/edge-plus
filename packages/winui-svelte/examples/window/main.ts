import type { ExampleHost } from "../harness/contract.ts";
import WindowApp from "./WindowApp.svelte";
import { fileURLToPath } from "node:url";
import { mountControl } from "winui-svelte";

// The window example: a titled window with a custom title bar and a backdrop
// (--backdrop acrylic|mica|micaAlt|none), opened off screen and unactivated
// (examples/window/run.ps1 captures it; test/window.test.ts checks it).
const iconFile = fileURLToPath(new URL("../../../../app/panel-tauri/src-tauri/icons/icon.ico", import.meta.url));

export function start(host: ExampleHost): void {
  mountControl(WindowApp, {
    host,
    icon: iconFile
  });
}
