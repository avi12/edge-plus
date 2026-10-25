import { spawn } from "node:child_process";
import { readFileSync } from "node:fs";
import path from "node:path";
import { createInterface } from "node:readline";

// A stand-in for the core, to work on the window alone: it opens the window on
// a saved View (fixture-view.json, or the file named), answers every action
// with success and prints what the window sends. Given a window kit (pnpm
// build:window), it runs the kit's Node and bundle instead of the sources.
//
//   node ui/dev/fixture.ts [view.json] [dist/window]

// Far left of any monitor; PrintWindow still captures it there.
const offScreen = {
  x: -8000,
  y: 0,
  width: 560,
  height: 1700
};
const viewFile = process.argv[2] ?? path.join(import.meta.dirname, "fixture-view.json");
const kit = process.argv[3];
const saved: unknown = JSON.parse(readFileSync(viewFile, "utf8"));
// The core would name the icon it wrote; here it is the checkout's.
const view = {
  ...typeof saved === "object" && saved !== null ? saved : {},
  icon: path.join(import.meta.dirname, "..", "..", "src-tauri", "icons", "icon.ico")
};
const [node, entry] = kit === undefined
  ? [process.execPath, path.join(import.meta.dirname, "..", "main.ts")]
  : [path.join(kit, "node.exe"), path.join(kit, "main.mjs")];
const window = spawn(node, [entry], { stdio: ["pipe", "pipe", "inherit"] });

function send(message: object): void {
  window.stdin.write(`${JSON.stringify(message)}\n`);
}

// EDGEPLUS_WINDOW_BACKGROUND=1: an unattended capture, opened off screen and
// without focus, so it never interrupts the user.
const isBackgroundRun = process.env["EDGEPLUS_WINDOW_BACKGROUND"] === "1";
if (isBackgroundRun) {
  send({
    type: "place",
    bounds: offScreen
  });
}

send({
  type: "view",
  view
});
createInterface({ input: window.stdout }).on("line", line => {
  process.stdout.write(`window: ${line}\n`);
  const message: unknown = JSON.parse(line);
  const isAction = typeof message === "object" && message !== null && "id" in message;
  if (isAction) {
    send({
      type: "reply",
      id: message.id,
      error: null
    });
  }
});
window.on("exit", code => {
  process.exit(code ?? 0);
});
