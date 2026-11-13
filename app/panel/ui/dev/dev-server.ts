import { execFileSync, spawn } from "node:child_process";
import { copyFileSync, mkdirSync, rmSync, watch } from "node:fs";
import path from "node:path";
import { createInterface } from "node:readline";

import { type BuildResult, type CoreProcess, createCoreRunner } from "./core-runner.ts";
import { createWindowSupervisor, pipeNameOf, servePipe, spawnWindowProcess } from "./window-supervisor.ts";

// The panel's dev server: the window stays up and the side that drives it
// reloads under it, so an edit shows without a restart.
//
//   pnpm dev          the real Rust core, built and started by this process, on
//                     the machine itself.
//   pnpm dev:sandbox  the same core in a sandbox: its data folder is
//                     .verify/dev-data (not yours) and Edge is the installed
//                     folder read for its version only, so it never injects
//                     into or changes the Edge you use. It still writes the
//                     start-up entry if you switch that on.
//
// WARNING, pnpm dev: at start it injects the split DLL into EVERY Edge browser
// process on the pinned build, and it changes the settings, the start-up entry
// and Edge's themes like the installed panel. Both refuse to start while any
// edgeplus.exe is running (the single-instance guard would hand its start-up
// to that one), naming the pid.
//
// How the window outlives the core: this process owns the window's process
// (window-supervisor.ts) and serves a named pipe. The real core is started
// with EDGEPLUS_WINDOW_COMMAND naming relay.ts, so it "opens the window" by
// starting a relay to that pipe. Saving a Rust file builds the core; a build
// that works replaces the core this process started (and no other), one that
// fails leaves the old core running and prints the compiler's words. The core
// that goes takes its relay with it and the window keeps its last View until
// the new core connects and sends a fresh one. The core runs from a copy of
// the built exe under .verify/, so cargo can write the next build while it
// runs.
//
// Press Enter to build and start the core again after it ended.

const SANDBOX_FLAG = "--sandbox";
const IS_LIVE = !process.argv.includes(SANDBOX_FLAG);
const APP_ROOT = path.join(import.meta.dirname, "..", "..");
const CORE_ROOT = path.join(APP_ROOT, "src-tauri");
const WINDOW_ENTRY = path.join(APP_ROOT, "ui", "main.ts");
const RELAY_ENTRY = path.join(import.meta.dirname, "relay.ts");
const NODE_FLAGS = ["--disable-warning=ExperimentalWarning"];
const BUILT_CORE = path.join(CORE_ROOT, "target", "debug", "edgeplus.exe");
const CORE_COPIES_ROOT = path.join(APP_ROOT, ".verify", "dev-core");
const SANDBOX_DATA = path.join(APP_ROOT, ".verify", "dev-data");
const EDGE_APP_PATHS_KEY = String.raw`HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe`;
const CORE_PROCESS_NAME = "edgeplus.exe";
const CORE_SOURCE_FOLDERS = [path.join(CORE_ROOT, "src"), path.join(CORE_ROOT, "edge-native")];
const CORE_SOURCE_PATTERN = /\.(rs|toml)$/;
const CORE_DEBOUNCE_MILLISECONDS = 400;
const CORE_ENVIRONMENT_VARIABLE = "EDGEPLUS_WINDOW_COMMAND";

function log(line: string) {
  process.stdout.write(`dev: ${line}\n`);
}

function logFailure(error: Error) {
  log(error.stack ?? error.message);
}

/** Calls `onChange` once after a burst of saves; `isRelevant` sees each file name. */
function watchFolder({
  folder,
  isRelevant,
  delay,
  onChange
}: {
  folder: string;
  isRelevant: (file: string) => boolean;
  delay: number;
  onChange: () => void;
}) {
  let timer: ReturnType<typeof setTimeout> | undefined;
  watch(folder, { recursive: true }, (_event, file) => {
    const isChange = file !== null && isRelevant(file);
    if (isChange) {
      clearTimeout(timer);
      timer = setTimeout(onChange, delay);
    }
  });
}

const supervisor = createWindowSupervisor({
  spawnWindow: () =>
    spawnWindowProcess({
      node: process.execPath,
      entry: WINDOW_ENTRY,
      environment: process.env
    }),
  log: line => log(line)
});

/** Called with each line the user types, so Enter can bring back what has ended. */
function onEnter(handler: () => void) {
  createInterface({ input: process.stdin }).on("line", handler);
}

function findRunningCore() {
  const listing = execFileSync("tasklist", ["/FI", `IMAGENAME eq ${CORE_PROCESS_NAME}`, "/FO", "CSV", "/NH"], {
    encoding: "utf8"
  });
  const match = /^"edgeplus\.exe","(\d+)"/imu.exec(listing);
  return match === null ? undefined : Number(match[1]);
}

function buildCore() {
  return new Promise<BuildResult>(resolve => {
    const output: string[] = [];
    const build = spawn("cargo", ["build", "--manifest-path", path.join(CORE_ROOT, "Cargo.toml")], {
      env: {
        ...process.env,
        CARGO_TERM_COLOR: "never"
      },
      windowsHide: true
    });
    build.stdout.on("data", chunk => output.push(String(chunk)));
    build.stderr.on("data", chunk => output.push(String(chunk)));
    build.on("exit", code =>
      resolve({
        isOk: code === 0,
        output: output.join("")
      })
    );
  });
}

/** Where the sandboxed core keeps its data, and the Edge folder it reads a version from. */
function sandboxEnvironment() {
  mkdirSync(SANDBOX_DATA, { recursive: true });
  return {
    LOCALAPPDATA: SANDBOX_DATA,
    EDGEPLUS_SIMULATE_EDGE_DIR: installedEdgeFolder()
  };
}

function installedEdgeFolder() {
  const listing = execFileSync("reg", ["query", EDGE_APP_PATHS_KEY, "/ve"], { encoding: "utf8" });
  const match = /REG_SZ\s+(.+\.exe)\s*$/imu.exec(listing);
  if (match === null) {
    throw new Error("Edge is not installed: the sandboxed core reads its version from the install folder");
  }

  return path.dirname(match[1].trim());
}

function startCore(pipeName: string): CoreProcess {
  const folder = path.join(CORE_COPIES_ROOT, String(Date.now()));
  mkdirSync(folder, { recursive: true });
  const copy = path.join(folder, CORE_PROCESS_NAME);
  copyFileSync(BUILT_CORE, copy);
  const child = spawn(copy, [], {
    stdio: ["ignore", "pipe", "pipe"],
    env: {
      ...process.env,
      ...(IS_LIVE ? {} : sandboxEnvironment()),
      [CORE_ENVIRONMENT_VARIABLE]: JSON.stringify([process.execPath, ...NODE_FLAGS, RELAY_ENTRY, pipeName])
    },
    windowsHide: true
  });
  for (const stream of [child.stdout, child.stderr]) {
    createInterface({ input: stream }).on("line", line => log(`core: ${line}`));
  }

  const exited = new Promise<void>(resolve => {
    child.once("exit", () => {
      rmSync(folder, {
        recursive: true,
        force: true
      });
      resolve();
    });
  });
  return {
    pid: child.pid,
    exited,
    async stop() {
      child.kill();
      await exited;
    }
  };
}

async function startCoreServer() {
  log(
    IS_LIVE
      ? "LIVE core: it injects into every Edge on the pinned build and changes real settings. Ctrl+C to stop."
      : "sandboxed core: your Edge and settings are untouched. Ctrl+C to stop."
  );
  const pipeName = pipeNameOf(process.pid);
  await servePipe({
    supervisor,
    pipeName
  });
  const runner = createCoreRunner({
    build: buildCore,
    start: () => startCore(pipeName),
    findRunning: findRunningCore,
    log
  });
  await runner.begin();
  for (const folder of CORE_SOURCE_FOLDERS) {
    watchFolder({
      folder,
      isRelevant: file => CORE_SOURCE_PATTERN.test(file),
      delay: CORE_DEBOUNCE_MILLISECONDS,
      onChange: () => runner.rebuild().catch(logFailure)
    });
  }

  onEnter(() => runner.rebuild().catch(logFailure));
  return runner;
}

let runningCore: Awaited<ReturnType<typeof startCoreServer>> | null = null;
try {
  runningCore = await startCoreServer();
} catch (error) {
  log(error instanceof Error ? error.message : String(error));
  process.exit(1);
}

log("ready; the window hot reloads, Rust saves rebuild the core");
process.once("SIGINT", () => {
  Promise.resolve(runningCore?.stop()).finally(() => {
    supervisor.stop();
    process.exit(0);
  });
});
