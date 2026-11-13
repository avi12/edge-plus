import { spawnSync } from "node:child_process";
import path from "node:path";

import { startRealCore } from "#test/real-core.ts";

// Opens the real core in a sandbox (test/real-core.ts), saves a PNG of its
// window and closes it. Run it on a desktop of its own
// (scripts/desktop/run-on-desktop.ps1) so the window never reaches the
// person's screen. Run `cargo build` first.
//
//   node scripts/capture.ts <file.png>

const CAPTURE_SCRIPT = path.join(import.meta.dirname, "desktop", "capture.ps1");
const SETTLE_MILLISECONDS = 4000;

const TARGET = process.argv[2];
const IS_TARGET_MISSING = TARGET === undefined;
if (IS_TARGET_MISSING) {
  throw new Error("name the PNG file to write");
}

const core = await startRealCore();
try {
  await new Promise(resolve => setTimeout(resolve, SETTLE_MILLISECONDS));
  const captured = spawnSync(
    "powershell.exe",
    [
      "-NoProfile",
      "-ExecutionPolicy",
      "Bypass",
      "-File",
      CAPTURE_SCRIPT,
      "-ProcessId",
      String(core.windowPid),
      "-Path",
      path.resolve(TARGET)
    ],
    { encoding: "utf8" }
  );
  process.stdout.write(`${captured.stdout}${captured.stderr}`);
  process.exitCode = captured.status ?? 1;
} finally {
  core.stop();
}
