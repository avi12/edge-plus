import { svelteWinui } from "svelte-winui/vite";
import { defineConfig } from "vite";

// What the window's dev server (ui/dev/window-modules.ts) reads: Svelte on
// WinUI, in one line.
export default defineConfig({ plugins: [svelteWinui()] });
