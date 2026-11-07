import { defineConfig } from "vite";
import { winuiSvelte } from "winui-svelte/vite";

// What the window's dev server (ui/dev/window-modules.ts) reads: Svelte on
// WinUI, in one line.
export default defineConfig({ plugins: [winuiSvelte()] });
