// The same rules as the Edge+ panel's (its eslint.config.js): one style for
// the repository's TypeScript.
import appConfig from "../../app/panel-tauri/eslint.config.js";

export default [
  ...appConfig,
  {
    // As the panel's ui/window/native.ts: the one file that binds DLL exports.
    files: ["src/window/native.ts"],
    rules: {
      "no-restricted-imports": "off"
    }
  }
];
