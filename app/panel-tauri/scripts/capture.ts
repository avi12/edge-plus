import { startFakeCore } from "#src/dev/fake-core.ts";
import { spawnSync } from "node:child_process";
import path from "node:path";

// Opens the window against a stand-in core, saves a PNG of it and closes it.
// Run it on a desktop of its own (scripts/desktop/run-on-desktop.ps1) so the
// window never reaches the person's screen.
//
//   node scripts/capture.ts <file.png> [dist/window]

const CAPTURE_SCRIPT = path.join(import.meta.dirname, "desktop", "capture.ps1");
const SETTLE_MILLISECONDS = 4000;
const OPEN_TIMEOUT_MILLISECONDS = 90_000;

const TARGET = process.argv[2];
const IS_TARGET_MISSING = TARGET === undefined;
if (IS_TARGET_MISSING) {
  throw new Error("name the PNG file to write");
}

const KIT_ARGUMENT = 3;
const KIT = process.argv[KIT_ARGUMENT];
const core = KIT === undefined
  ? startFakeCore()
  : startFakeCore({
    node: path.join(KIT, "node.exe"),
    entry: path.join(KIT, "main.mjs")
  });
const timeout = setTimeout(() => {
  process.stderr.write("the window did not open in time\n");
  void core.stop();
}, OPEN_TIMEOUT_MILLISECONDS);
await core.waitFor("bounds");
clearTimeout(timeout);
await new Promise(resolve => setTimeout(resolve, SETTLE_MILLISECONDS));
const captured = spawnSync("powershell.exe", [
  "-NoProfile",
  "-ExecutionPolicy",
  "Bypass",
  "-File",
  CAPTURE_SCRIPT,
  "-ProcessId",
  String(core.child.pid),
  "-Path",
  path.resolve(TARGET)
], { encoding: "utf8" });
process.stdout.write(`${captured.stdout}${captured.stderr}`);
process.exitCode = await core.stop() ?? 1;
