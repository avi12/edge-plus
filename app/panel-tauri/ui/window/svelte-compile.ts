import { loadWinuiCatalogue } from "../../../../packages/svelte-winui/src/css/catalogue.ts";
import { compileSheetModule, cssHash } from "../../../../packages/svelte-winui/src/css/sheet-module.ts";
import { stripTypeScriptTypes } from "node:module";
import path from "node:path";
import { compile, compileModule } from "svelte/compiler";

// How the window's .svelte components (and the .svelte.ts modules that use
// runes) become JavaScript in the window kit (scripts/build-window.ts) and in
// tests. Components render through the WinUI renderer unless told another one
// (the tests render through one without WinUI); the dev server
// (ui/dev/window-modules.ts) names it the same way. A component's <style>
// becomes WinUI Styles: its sheet is registered by statements appended to the
// component (the dev server does it with the winuiCss Vite plugin instead).

export const winuiRenderer = path.join(import.meta.dirname, "svelte-renderer.ts");
export const cssRuntime = path.join(import.meta.dirname, "..", "..", "..", "..", "packages", "svelte-winui", "src", "css", "runtime.ts");
const componentExtension = ".svelte";
const runesExtension = ".svelte.ts";
const projectRoot = path.join(import.meta.dirname, "..", "..");

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

const resources = loadWinuiCatalogue();

export function compileSvelte(filename: string, source: string, renderer = winuiRenderer): string {
  if (filename.endsWith(runesExtension)) {
    return compileModule(stripTypeScriptTypes(source), {
      filename,
      generate: "client"
    }).js.code;
  }

  const component = compile(source, {
    filename,
    generate: "client",
    runes: true,
    cssHash,
    experimental: {
      customRenderer: ({ filename: name }) => importPath(name, renderer)
    }
  }).js.code;
  const sheet = compileSheetModule({
    filename,
    source,
    sheetId: path.relative(projectRoot, filename).replaceAll("\\", "/"),
    resources,
    runtimeModule: importPath(filename, cssRuntime)
  });
  return sheet === null ? component : `${component}\n${sheet.code}`;
}
