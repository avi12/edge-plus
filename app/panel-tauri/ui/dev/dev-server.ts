import { type BuildResult, type CoreProcess, createCoreRunner } from "./core-runner.ts";
import type * as FakeHostModule from "./fake-host.ts";
import { createWindowSupervisor, pipeNameOf, servePipe, spawnWindowProcess } from "./window-supervisor.ts";
import { CoreMessageType } from "#src/protocol.g.ts";
import { execFileSync, spawn } from "node:child_process";
import { copyFileSync, mkdirSync, rmSync, watch } from "node:fs";
import path from "node:path";
import { createInterface } from "node:readline";
import { pathToFileURL } from "node:url";

// The panel's dev server: the window stays up and the side that drives it
// reloads under it, so an edit shows without a restart.
//
//   pnpm dev        a fake core inside this process. Nothing on the machine is
//                   touched: it sends ui/dev/fixture-view.json and answers
//                   every Action with success. Saving fixture-view.json or
//                   fake-host.ts starts the fake core's session again over the
//                   same link; saving a window .svelte or .ts hot reloads in
//                   the one window (ui/main.ts's Vite server).
//   pnpm dev:real   the real Rust core, built and started by this process.
//
// WARNING, pnpm dev:real: the real core runs. At start it injects the split
// DLL into EVERY Edge browser process on the pinned build, and it changes the
// settings, the start-up entry and Edge's themes like the installed panel.
// Never run it against an Edge you are using. It refuses to start while any
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
// Press Enter to open the window again after closing it (pnpm dev), or to
// build and start the core again after it ended (pnpm dev:real).

const REAL_FLAG = "--real";
const IS_REAL = process.argv.includes(REAL_FLAG);
const IS_BACKGROUND = process.env["EDGEPLUS_WINDOW_BACKGROUND"] === "1";
const APP_ROOT = path.join(import.meta.dirname, "..", "..");
const CORE_ROOT = path.join(APP_ROOT, "src-tauri");
const WINDOW_ENTRY = path.join(APP_ROOT, "ui", "main.ts");
const RELAY_ENTRY = path.join(import.meta.dirname, "relay.ts");
const NODE_FLAGS = ["--disable-warning=ExperimentalWarning"];
const FAKE_HOST_MODULE = path.join(import.meta.dirname, "fake-host.ts");
const FAKE_HOST_WATCHED_FILES = ["fixture-view.json", "fake-host.ts"];
const BUILT_CORE = path.join(CORE_ROOT, "target", "debug", "edgeplus.exe");
const CORE_COPIES_ROOT = path.join(APP_ROOT, ".verify", "dev-core");
const CORE_PROCESS_NAME = "edgeplus.exe";
const CORE_SOURCE_FOLDERS = [path.join(CORE_ROOT, "src"), path.join(CORE_ROOT, "edge-native")];
const CORE_SOURCE_PATTERN = /\.(rs|toml)$/;
const FAKE_HOST_DEBOUNCE_MILLISECONDS = 150;
const CORE_DEBOUNCE_MILLISECONDS = 400;
const CORE_ENVIRONMENT_VARIABLE = "EDGEPLUS_WINDOW_COMMAND";

function log(line: string) {
  process.stdout.write(`dev: ${line}\n`);
}

function logFailure(error: Error) {
  log(error.stack ?? error.message);
}

/** Calls `onChange` once after a burst of saves; `isRelevant` sees each file name. */
function watchFolder({ folder, isRelevant, delay, onChange }: {
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
  spawnWindow: () => spawnWindowProcess({
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

function startFake() {
  let host: ReturnType<typeof FakeHostModule.createFakeHost>;
  let placeWindow: (typeof FakeHostModule)["OFF_SCREEN_BOUNDS"] | undefined;
  const link = {
    send: (line: string) => host.receive(line),
    close: () => log("window closed; press Enter to open it again")
  };
  let connection = supervisor.connect(link);

  async function startSession() {
    try {
      const module: typeof FakeHostModule = await import(`${pathToFileURL(FAKE_HOST_MODULE).href}?reload=${Date.now()}`);
      host = module.createFakeHost();
      placeWindow = module.OFF_SCREEN_BOUNDS;
    } catch (error) {
      log(`the fake core did not start: ${error instanceof Error ? error.stack : String(error)}`);
      return;
    }

    host.attach(line => connection.write(line));

    const shouldPlaceWindow = IS_BACKGROUND && placeWindow !== undefined;
    if (shouldPlaceWindow) {
      host.request(CoreMessageType.place, placeWindow);
    }

    host.pushView();
    log("fake core: session started");
  }

  watchFolder({
    folder: import.meta.dirname,
    isRelevant: file => FAKE_HOST_WATCHED_FILES.includes(path.basename(file)),
    delay: FAKE_HOST_DEBOUNCE_MILLISECONDS,
    onChange: () => startSession().catch(logFailure)
  });
  onEnter(() => {
    connection = supervisor.connect(link);
    startSession().catch(logFailure);
  });
  return startSession();
}

function findRunningCore() {
  const listing = execFileSync("tasklist", ["/FI", `IMAGENAME eq ${CORE_PROCESS_NAME}`, "/FO", "CSV", "/NH"], { encoding: "utf8" });
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
    build.on("exit", code => resolve({
      isOk: code === 0,
      output: output.join("")
    }));
  });
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

async function startReal() {
  log("REAL core: it injects into every Edge on the pinned build and changes real settings. Ctrl+C to stop.");
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

let runningCore: Awaited<ReturnType<typeof startReal>> | null = null;
try {
  if (IS_REAL) {
    runningCore = await startReal();
  } else {
    await startFake();
  }
} catch (error) {
  log(error instanceof Error ? error.message : String(error));
  process.exit(1);
}

log(IS_REAL ? "ready; the window hot reloads, Rust saves rebuild the core" : "ready; the window hot reloads, fixture-view.json restarts the fake core's session");
process.once("SIGINT", () => {
  Promise.resolve(runningCore?.stop()).finally(() => {
    supervisor.stop();
    process.exit(0);
  });
});
