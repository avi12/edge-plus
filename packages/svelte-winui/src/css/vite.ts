import { loadWinuiCatalogue } from "./catalogue.ts";
import { compileSheetModule, SheetError, sheetSuffix } from "./sheet-module.ts";
import { formatDiagnostic } from "./translate.ts";
import { readFileSync } from "node:fs";
import path from "node:path";
import type { CompileOptions } from "svelte/compiler";
import type { Plugin } from "vite";

// The Vite plugin that turns every component's <style> into the module that
// registers its WinUI Styles. For `Card.svelte` it adds
// `import "Card.svelte-winui-css.js"` to the compiled component; that module is
// the translated sheet (see sheet-module.ts). Editing a <style> reloads that
// module as well (hotUpdate), so the open window restyles in place.
//
//   plugins: [svelte({ compilerOptions: { cssHash } }), winuiCss({ root, compilerOptions })]
//
// `compilerOptions` must be the ones the component is compiled with, and its
// cssHash the one exported from sheet-module.ts, so the scoping class the
// sheet's selectors carry is the one the elements get.

export interface WinuiCssOptions {
  // Sheet ids and diagnostics are paths relative to this; default Vite's root.
  root?: string;
  compilerOptions?: CompileOptions;
  // The module that exports registerStyleSheet, as the window imports it.
  runtimeModule?: string;
  // What a <style> WinUI cannot say does: "fail" the load, or "report" the
  // line on stderr and register nothing, so an open window stays up (dev).
  onError?: "fail" | "report";
}

const pluginName = "svelte-winui:css";
const defaultRuntimeModule = "svelte-winui/css";
const queryStart = "?";
const svelteExtension = ".svelte";

const styleOpening = "<style";
const styleBlocks = /<style[\s\S]*?<\/style>/gu;
function withoutStyle(source: string): string {
  return source.replace(styleBlocks, styleOpening);
}

function isComponentId(id: string): boolean {
  return id.endsWith(svelteExtension) && !id.includes(queryStart);
}

function isSheetId(id: string): boolean {
  return id.endsWith(`${svelteExtension}${sheetSuffix}`);
}

export function winuiCss(options: WinuiCssOptions): Plugin {
  const resources = loadWinuiCatalogue();
  const runtimeModule = options.runtimeModule ?? defaultRuntimeModule;
  let root = options.root ?? "";
  // Each component as last compiled, less its <style> blocks.
  const withoutStyles = new Map<string, string>();
  return {
    name: pluginName,
    enforce: "post",
    configResolved(config) {
      root = options.root ?? config.root;
    },
    transform(code, id) {
      if (!isComponentId(id)) {
        return null;
      }

      const source = readFileSync(id, "utf8");
      withoutStyles.set(id, withoutStyle(source));
      const hasStyle = source.includes(styleOpening);
      if (!hasStyle) {
        return null;
      }

      return {
        code: `${code}\nimport ${JSON.stringify(`${id}${sheetSuffix}`)};\n`,
        map: null
      };
    },
    // The sheet module is not the component's file, so a change to the
    // component does not reach it by itself. A change to the <style> alone is
    // the sheet's: the component is left running, with its state, and its live
    // elements restyle.
    hotUpdate({ file, modules }) {
      const sheet = this.environment.moduleGraph.getModuleById(`${file}${sheetSuffix}`);
      if (sheet === undefined) {
        return undefined;
      }

      const previous = withoutStyles.get(file);
      const current = withoutStyle(readFileSync(file, "utf8"));
      withoutStyles.set(file, current);
      const isStyleOnlyChange = previous === current;
      return isStyleOnlyChange ? [sheet] : [...modules, sheet];
    },
    // The import a component gets may be root-relative (`/window/a.svelte-winui-css.js`):
    // the sheet's id has to be the file's own path, which `load` reads.
    async resolveId(source, importer, resolveOptions) {
      if (!isSheetId(source)) {
        return null;
      }

      const component = source.slice(0, -sheetSuffix.length);
      const resolved = await this.resolve(component, importer, {
        ...resolveOptions,
        skipSelf: true
      });
      return resolved === null ? null : `${resolved.id}${sheetSuffix}`;
    },
    load(id) {
      if (!isSheetId(id)) {
        return null;
      }

      const filename = id.slice(0, -sheetSuffix.length);
      this.addWatchFile(filename);
      try {
        const module = compileSheetModule({
          filename,
          source: readFileSync(filename, "utf8"),
          sheetId: path.relative(root, filename).replaceAll("\\", "/"),
          resources,
          runtimeModule,
          compilerOptions: options.compilerOptions,
          isHot: true
        });
        for (const diagnostic of module?.diagnostics ?? []) {
          this.warn(formatDiagnostic(diagnostic));
        }

        return module === null ? "export default null;" : module.code;
      } catch (error) {
        const isReported = error instanceof SheetError && options.onError === "report";
        if (isReported) {
          process.stderr.write(`${error.message}\n`);
          return "export default null;";
        }

        if (error instanceof SheetError) {
          this.error(error.message);
        }

        throw error;
      }
    }
  };
}
