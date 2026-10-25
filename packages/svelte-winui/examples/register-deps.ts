import { registerHooks } from "node:module";
import path from "node:path";
import { pathToFileURL } from "node:url";

// The examples run before the package has a node_modules of its own: a bare
// import that does not resolve from an example's folder is retried from Edge+'s
// panel, which holds Svelte and dynwinrt. `node --import ./register-deps.ts`.

const panelFolder = path.join(import.meta.dirname, "..", "..", "..", "app", "panel-tauri");
const panelAnchor = pathToFileURL(path.join(panelFolder, "ui", "main.ts")).href;
const bareSpecifier = /^(?![./#]|[a-z]+:)/u;

registerHooks({
  resolve(specifier, context, nextResolve) {
    try {
      return nextResolve(specifier, context);
    } catch (error) {
      const isBare = bareSpecifier.test(specifier);
      if (!isBare) {
        throw error;
      }

      return nextResolve(specifier, {
        ...context,
        parentURL: panelAnchor
      });
    }
  }
});
