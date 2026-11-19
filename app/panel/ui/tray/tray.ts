import path from "node:path";

import { defineMenu, MenuItemKind } from "winui-svelte/tray/menu";
import { createStandaloneTray, menuIcons } from "winui-svelte/tray/standalone";

import { TrayMessageType } from "../protocol.g.ts";

// The notification-area icon, a process of its own that the core starts and
// ends (its stdin closes when the core goes): the same winui-svelte tray
// AnySTT uses. Windows' own popup menu, with Open and Exit and revert; a pick
// goes to the core as one JSON line.

const TOOLTIP = "Edge+";
const ICON_FILE = "edgeplus.ico";
const ICON_DIRECTORY = "icons";
const SOURCE_EXTENSION = ".ts";
const KIT_ASSETS_FOLDER = "tray";
const OWN_REQUEST_IDENTIFIER = 0;

const IS_FROM_SOURCE = path.extname(import.meta.filename) === SOURCE_EXTENSION;
const assets = IS_FROM_SOURCE ? import.meta.dirname : path.join(import.meta.dirname, KIT_ASSETS_FOLDER);

function iconOf(name: string) {
  return { tinted: path.join(assets, ICON_DIRECTORY, `${name}.png`) };
}

function tell(type: string) {
  process.stdout.write(`${JSON.stringify({ id: OWN_REQUEST_IDENTIFIER, type, timestamp: Date.now() })}\n`);
}

const open = () => tell(TrayMessageType.open);

createStandaloneTray({
  icon: path.join(assets, ICON_FILE),
  tooltip: TOOLTIP,
  ondoubleclick: open,
  extras: [menuIcons],
  menu: defineMenu([
    {
      kind: MenuItemKind.COMMAND,
      id: "open",
      label: "Open",
      icon: iconOf("open"),
      onselect: open
    },
    {
      kind: MenuItemKind.SEPARATOR,
      id: "line"
    },
    {
      kind: MenuItemKind.COMMAND,
      id: "exit",
      label: "Exit and revert Edge+",
      icon: iconOf("exit"),
      onselect: () => tell(TrayMessageType.exitAndRevert)
    }
  ])
});

// The core is gone: the icon goes with it.
process.stdin.resume();
process.stdin.on("end", () => process.exit(0));
process.stdin.on("close", () => process.exit(0));
