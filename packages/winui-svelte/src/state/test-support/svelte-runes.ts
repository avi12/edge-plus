import { readFileSync } from "node:fs";
import { registerHooks, stripTypeScriptTypes } from "node:module";
import { fileURLToPath } from "node:url";
import { compileModule } from "svelte/compiler";

// Lets a test import `.svelte.ts` modules (runes) from source: Svelte resolves
// to its browser build, whose `$state` is reactive, and a rune module is
// compiled as it loads. Import this before the module under test.

const svelteFolder = /[\\/]node_modules[\\/]svelte[\\/]/u;
const runesExtension = ".svelte.ts";

registerHooks({
  resolve(specifier, context, nextResolve) {
    const isSvelte = specifier === "svelte" || specifier.startsWith("svelte/") || svelteFolder.test(context.parentURL ?? "");
    if (!isSvelte) {
      return nextResolve(specifier, context);
    }

    return nextResolve(specifier, {
      ...context,
      conditions: [...context.conditions, "browser"]
    });
  },
  load(url, context, nextLoad) {
    if (!url.endsWith(runesExtension)) {
      return nextLoad(url, context);
    }

    const filename = fileURLToPath(url);
    return {
      format: "module",
      source: compileModule(stripTypeScriptTypes(readFileSync(filename, "utf8")), {
        filename,
        generate: "client"
      }).js.code,
      shortCircuit: true
    };
  }
});
