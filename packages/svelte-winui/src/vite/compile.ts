import { describeError } from "./diagnostics.ts";
import { stripTypeScriptTypes } from "node:module";
import path from "node:path";
import { compile, compileModule } from "svelte/compiler";

// How .svelte components (and the .svelte.ts modules that use runes) become
// JavaScript where Vite is not in the way: a bundler building a release
// (esbuild) and tests. Components render through the WinUI renderer unless
// told another one (the tests render through one without WinUI). The dev
// server does the same through the Vite plugin (plugin.ts).

// The module a component imports its renderer from: a package specifier, or a
// file, which the component then imports by relative path.
export const winuiRenderer = "svelte-winui/host";
const componentExtension = ".svelte";
const runesExtension = ".svelte.ts";

function importPath(filename: string, module: string): string {
  const relative = path.relative(path.dirname(filename), module).replaceAll("\\", "/");
  return relative.startsWith(".") ? relative : `./${relative}`;
}

// How a component at `filename` imports the renderer.
export function rendererImport(filename: string, renderer: string): string {
  return path.isAbsolute(renderer) ? importPath(filename, renderer) : renderer;
}

export function isSvelteFile(filename: string): boolean {
  return filename.endsWith(componentExtension) || filename.endsWith(runesExtension);
}

function compileSource(filename: string, source: string, renderer: string): string {
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
      customRenderer: ({ filename: component }) => rendererImport(component, renderer)
    }
  }).js.code;
}

// A compile error is thrown as one that names the file and line, which a
// bundler prints as it is.
export function compileSvelte(filename: string, source: string, renderer = winuiRenderer): string {
  try {
    return compileSource(filename, source, renderer);
  } catch (error) {
    throw new Error(describeError(error), { cause: error });
  }
}
