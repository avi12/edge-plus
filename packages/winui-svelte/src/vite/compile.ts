import { compileSheetModule, cssHash, SheetError } from "../css/runtime/sheet-module.ts";
import { loadWinuiCatalogue } from "../css/translate/catalogue.ts";
import { describeError, errorOf } from "./diagnostics.ts";
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
export const winuiRenderer = "winui-svelte/host";
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

export interface CompileOptions {
  // The module a component imports its renderer from.
  renderer?: string;
  // Sheet ids and their diagnostics are paths relative to this.
  root?: string;
  // The module that exports registerStyleSheet, as the compiled component
  // imports it.
  cssRuntime?: string;
}

const winuiCssRuntime = "winui-svelte/css";
const resources = loadWinuiCatalogue();

function compileSource(filename: string, source: string, options: CompileOptions): string {
  if (filename.endsWith(runesExtension)) {
    return compileModule(stripTypeScriptTypes(source), {
      filename,
      generate: "client"
    }).js.code;
  }

  const renderer = options.renderer ?? winuiRenderer;
  const component = compile(source, {
    filename,
    generate: "client",
    runes: true,
    cssHash,
    experimental: {
      customRenderer: ({ filename: name }) => rendererImport(name, renderer)
    }
  }).js.code;
  // The dev server registers the sheet through the Vite plugin (css/vite.ts);
  // here its statements are appended to the component.
  const sheet = compileSheetModule({
    filename,
    source,
    sheetId: path.relative(options.root ?? path.dirname(filename), filename).replaceAll("\\", "/"),
    resources,
    runtimeModule: options.cssRuntime ?? winuiCssRuntime
  });
  return sheet === null ? component : `${component}\n${sheet.code}`;
}

// A compile error is thrown as one that names the file and line, which a
// bundler prints as it is. A <style> WinUI cannot say fails the build with
// each rule's `file:line:column: property has no WinUI meaning (why)`.
export function compileSvelte(filename: string, source: string, options: CompileOptions = {}): string {
  try {
    return compileSource(filename, source, options);
  } catch (error) {
    const failure = errorOf(error);
    throw new Error(failure instanceof SheetError ? failure.message : describeError(failure), { cause: error });
  }
}
