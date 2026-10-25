import { svelteHooks } from "./svelte-loader.ts";
import { registerHooks } from "node:module";

// Run from source, the window compiles its .svelte files as Node loads them,
// for this thread.
registerHooks(svelteHooks());
