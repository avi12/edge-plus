import { stripTypeScriptTypes } from "node:module";
import path from "node:path";
import { compile, compileModule } from "svelte/compiler";

// How the window's .svelte components (and the .svelte.ts modules that use
// runes) become JavaScript in the window kit (scripts/build-window.ts) and in
// tests. Components render through the WinUI renderer unless told another one
// (the tests render through one without WinUI); the dev server
// (ui/dev/window-modules.ts) names it the same way.

export const winuiRenderer = path.join(import.meta.dirname, "svelte-renderer.ts");
const componentExtension = ".svelte";
const runesExtension = ".svelte.ts";

function importPath(filename: string, module: string): string {
  const relative = path.relative(path.dirname(filename), module).replaceAll("\\", "/");
  return relative.startsWith(".") ? relative : `./${relative}`;
}

// The WinUI renderer as a component at this path imports it.
export function rendererFor(filename: string): string {
  return importPath(filename, winuiRenderer);
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
