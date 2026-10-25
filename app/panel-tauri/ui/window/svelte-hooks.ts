import { compileSvelte, componentExtension, runesExtension } from "./svelte-compile.ts";
import { readFileSync } from "node:fs";
import { registerHooks } from "node:module";
import { fileURLToPath } from "node:url";

// Run from source, the window compiles its .svelte files as Node loads them,
// for this thread. Svelte resolves to its browser build, the one that mounts
// components.

const browserCondition = "browser";
const svelteFolder = /[\\/]node_modules[\\/]svelte[\\/]/u;

registerHooks({
  resolve(specifier, context, nextResolve) {
    const isSvelte = specifier === "svelte" || specifier.startsWith("svelte/") || svelteFolder.test(context.parentURL ?? "");
    if (!isSvelte) {
      return nextResolve(specifier, context);
    }

    return nextResolve(specifier, {
      ...context,
      conditions: [...context.conditions, browserCondition]
    });
  },
  load(url, context, nextLoad) {
    const isSvelte = url.endsWith(componentExtension) || url.endsWith(runesExtension);
    if (!isSvelte) {
      return nextLoad(url, context);
    }

    const filename = fileURLToPath(url);
    return {
      format: "module",
      source: compileSvelte(filename, readFileSync(filename, "utf8")),
      shortCircuit: true
    };
  }
});
