import extensionIdentity from "./extension-identity.json";
import { Command } from "./src/lib/feature";
import { defineConfig } from "wxt";

/** The public key pins the extension id, so the app's native host can allow exactly one origin */
const { publicKey } = extensionIdentity;

/** `Temporal`-free, but import attributes on JSON modules need 123 */
const MINIMUM_CHROMIUM_VERSION = "123";

/** A port no other local app was found listening on; 9333 is taken by a WebView2 app */
const CDP_PORT = 9871;

export default defineConfig({
  srcDir: "src",
  publicDir: "src/public",
  manifestVersion: 3,
  modules: ["@wxt-dev/module-svelte"],
  manifest: {
    name: "Fix Edge",
    description: "Live feature switches for Microsoft Edge, driven by the Fix Edge app",
    key: publicKey,
    minimum_chrome_version: MINIMUM_CHROMIUM_VERSION,
    permissions: ["nativeMessaging", "tabs", "storage", "contextMenus"],
    action: {
      default_title: "Split into a grid"
    },
    commands: {
      [Command.splitRight]: { suggested_key: { default: "Alt+Shift+Right" }, description: "Split the focused pane to the right" },
      [Command.splitDown]: { suggested_key: { default: "Alt+Shift+Down" }, description: "Split the focused pane downward" },
      [Command.unsplit]: { suggested_key: { default: "Alt+Shift+Up" }, description: "Merge every pane back into one window" }
    },
    homepage_url: "https://avi12.com"
  },
  webExt: {
    chromiumArgs: [`--remote-debugging-port=${CDP_PORT}`]
  }
});
