import { compileSvelte, isSvelteFile, winuiRenderer } from "./compile.ts";
import { readFileSync } from "node:fs";
import type { RegisterHooksOptions } from "node:module";
import { fileURLToPath } from "node:url";

// Node module hooks that run .svelte files from source: Svelte resolves to its
// browser build, the one that mounts components, and a .svelte file is
// compiled as it loads. A test registers them with its own renderer; the
// window itself runs from source through the Vite dev server instead
// (server.ts).

const browserCondition = "browser";
const svelteSpecifier = "svelte";
const svelteFolder = /[\\/]node_modules[\\/]svelte[\\/]/u;

function isSvelteImport(specifier: string, parentUrl: string | undefined): boolean {
  const isSveltePackage = specifier === svelteSpecifier || specifier.startsWith(`${svelteSpecifier}/`);
  return isSveltePackage || svelteFolder.test(parentUrl ?? "");
}

export function svelteHooks(renderer = winuiRenderer): RegisterHooksOptions {
  return {
    resolve(specifier, context, nextResolve) {
      if (!isSvelteImport(specifier, context.parentURL)) {
        return nextResolve(specifier, context);
      }

      return nextResolve(specifier, {
        ...context,
        conditions: [...context.conditions, browserCondition]
      });
    },
    load(url, context, nextLoad) {
      if (!isSvelteFile(url)) {
        return nextLoad(url, context);
      }

      const filename = fileURLToPath(url);
      return {
        format: "module",
        source: compileSvelte(filename, readFileSync(filename, "utf8"), renderer),
        shortCircuit: true
      };
    }
  };
}
