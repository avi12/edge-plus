import { stripTypeScriptTypes } from "node:module";
import path from "node:path";
import { compile, compileModule } from "svelte/compiler";

// How the window's .svelte components (and the .svelte.ts modules that use
// runes) become JavaScript, for both ways the window runs: from source
// (svelte-hooks.ts) and bundled into the window kit (scripts/build-window.ts).
// Components render through the WinUI renderer unless told another one (the
// tests render through one without WinUI).

export const winuiRenderer = path.join(import.meta.dirname, "svelte-renderer.ts");
const componentExtension = ".svelte";
const runesExtension = ".svelte.ts";

function importPath(filename: string, module: string): string {
  const relative = path.relative(path.dirname(filename), module).replaceAll("\\", "/");
  return relative.startsWith(".") ? relative : `./${relative}`;
}

export function isSvelteFile(filename: string): boolean {
  return filename.endsWith(componentExtension) || filename.endsWith(runesExtension);
}

export function compileSvelte(filename: string, source: string, renderer = winuiRenderer): string {
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
      customRenderer: ({ filename: component }) => importPath(component, renderer)
    }
  }).js.code;
}
