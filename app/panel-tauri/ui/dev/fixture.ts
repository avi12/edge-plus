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

const viewFile = process.argv[2] ?? path.join(import.meta.dirname, "fixture-view.json");
const kit = process.argv[3];
const view: unknown = JSON.parse(readFileSync(viewFile, "utf8"));
const [node, entry] = kit === undefined
  ? [process.execPath, path.join(import.meta.dirname, "..", "main.ts")]
  : [path.join(kit, "node.exe"), path.join(kit, "main.mjs")];
const window = spawn(node, [entry], { stdio: ["pipe", "pipe", "inherit"] });

function send(message: object): void {
  window.stdin.write(`${JSON.stringify(message)}\n`);
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
