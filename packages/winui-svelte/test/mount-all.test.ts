import { reportSchema } from "./mount-report.ts";
import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { existsSync } from "node:fs";
import path from "node:path";
import { test } from "node:test";
import { promisify } from "node:util";

// Every generated component mounted in a real WinUI window, which opens off
// screen and is never activated (mount-all.ts). Skipped where the Windows App
// SDK was not restored. The report is also what says how much was covered.

const run = promisify(execFile);
const script = path.join(import.meta.dirname, "mount-all.ts");
const bootstrap = path.join(import.meta.dirname, "..", ".winapp", "bin", process.arch, "Microsoft.WindowsAppRuntime.Bootstrap.dll");
const timeoutMilliseconds = 240_000;

test("every component mounts in a WinUI window", {
  skip: !existsSync(bootstrap),
  timeout: timeoutMilliseconds
}, async () => {
  const { stdout } = await run(process.execPath, ["--disable-warning=ExperimentalWarning", script], {
    env: {
      ...process.env,
      EDGEPLUS_WINDOW_BACKGROUND: "1"
    },
    timeout: timeoutMilliseconds,
    maxBuffer: 64 * 1024 * 1024
  });
  const lines = stdout.trim().split("\n");
  const report = reportSchema.parse(JSON.parse(lines[lines.length - 1] ?? "null"));
  process.stdout.write(`${JSON.stringify(report)}\n`);
  assert.deepEqual(report.failures.map(failure => `${failure.name} (${failure.step}): ${failure.message}`), []);
});
