import { stripTypeScriptTypes } from "node:module";
import path from "node:path";
import { compile, compileModule } from "svelte/compiler";

// How the window's .svelte components (and the .svelte.ts modules that use
// runes) become JavaScript, for both ways the window runs: from source
// (svelte-hooks.ts) and bundled into the window kit (scripts/build-window.ts).
// Components render through the WinUI renderer.

const rendererModule = path.join(import.meta.dirname, "svelte-renderer.ts");
export const componentExtension = ".svelte";
export const runesExtension = ".svelte.ts";

function rendererFor(filename: string): string {
  const relative = path.relative(path.dirname(filename), rendererModule).replaceAll("\\", "/");
  return relative.startsWith(".") ? relative : `./${relative}`;
}

export function compileSvelte(filename: string, source: string): string {
  if (filename.endsWith(runesExtension)) {
    return compileModule(stripTypeScriptTypes(source), {
      filename,
      generate: "client"
    }).js.code;
  }

  return compile(source, {
    filename,
    generate: "client",
    runes: true,
    experimental: {
      customRenderer: ({ filename: component }) => rendererFor(component)
    }
  }).js.code;
}
