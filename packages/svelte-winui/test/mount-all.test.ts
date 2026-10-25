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

interface Failure {
  name: string;
  step: string;
  message: string;
}

function isFailureList(value: unknown): value is Failure[] {
  return Array.isArray(value);
}

test("every component mounts in a WinUI window", { skip: !existsSync(bootstrap), timeout: timeoutMilliseconds }, async () => {
  const { stdout } = await run(process.execPath, ["--disable-warning=ExperimentalWarning", script], {
    env: { ...process.env, EDGEPLUS_WINDOW_BACKGROUND: "1" },
    timeout: timeoutMilliseconds,
    maxBuffer: 64 * 1024 * 1024
  });
  const lines = stdout.trim().split("\n");
  const report: unknown = JSON.parse(lines[lines.length - 1] ?? "null");
  assert.ok(typeof report === "object" && report !== null, "the window reported nothing");
  process.stdout.write(`${JSON.stringify(report)}\n`);
  const failures: unknown = "failures" in report ? report.failures : [];
  assert.ok(isFailureList(failures));
  assert.deepEqual(failures.map(failure => `${failure.name} (${failure.step}): ${failure.message}`), []);
});
