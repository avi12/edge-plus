// The same rules as the Edge+ panel's (its eslint.config.js): one style for
// the repository's TypeScript and Svelte.
import appConfig from "../../app/panel-tauri/eslint.config.js";
import { globalIgnores } from "eslint/config";

export default [
  ...appConfig,
  // Written by scripts/codegen from WinUI's metadata, not by hand.
  globalIgnores(["src/components/generated/"]),
  {
    // As the panel's ui/window/native.ts: the one file that binds DLL exports.
    files: ["src/window/native.ts"],
    rules: {
      "no-restricted-imports": "off"
    }
  }
];
