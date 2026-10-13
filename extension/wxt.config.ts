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
    permissions: ["nativeMessaging", "tabs", "storage", "contextMenus", "declarativeNetRequest", "favicon"],
    // Every site may be opened in a pane: the pane bridge runs in it, and the frame rule may lift its
    // refusal to be framed - inside split tabs only
    host_permissions: ["<all_urls>"],
    action: {
      default_title: "Split into a grid"
    },
    // Edge allows default keys on at most four commands, so the arrows go to moving between panes;
    // splitting has the toolbar popup and the page menu, and keys of the user's own choosing
    commands: {
      [Command.focusLeft]: { suggested_key: { default: "Alt+Shift+Left" }, description: "Move to the pane on the left" },
      [Command.focusRight]: { suggested_key: { default: "Alt+Shift+Right" }, description: "Move to the pane on the right" },
      [Command.focusUp]: { suggested_key: { default: "Alt+Shift+Up" }, description: "Move to the pane above" },
      [Command.focusDown]: { suggested_key: { default: "Alt+Shift+Down" }, description: "Move to the pane below" },
      [Command.splitRight]: { description: "Split the focused pane to the right" },
      [Command.splitDown]: { description: "Split the focused pane downward" },
      [Command.unsplit]: { description: "Merge every pane back into one window" }
    },
    homepage_url: "https://avi12.com"
  },
  webExt: {
    chromiumArgs: [`--remote-debugging-port=${CDP_PORT}`]
  }
});
