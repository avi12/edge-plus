import { type ChildProcess, execFileSync, spawn, spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { setTimeout as sleep } from "node:timers/promises";

import { ownProcessTree } from "./process-tree.ts";
import { exists } from "./uia.ts";

// The real Rust core, built by `cargo build`, in a sandbox: its data folder is
// a temporary LOCALAPPDATA and Edge is the installed folder read for its
// version only (EDGEPLUS_SIMULATE_EDGE_DIR), so it never injects into or
// changes the Edge in use. The window it opens is the one a user gets, placed
// off screen (EDGEPLUS_WINDOW_BACKGROUND) on the desktop the test runs on.

const APP_ROOT = path.join(import.meta.dirname, "..");
const CORE_EXE = path.join(APP_ROOT, "src-tauri", "target", "debug", "edgeplus.exe");
const EDGE_APP_PATHS_KEY = "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe";
const SETTINGS_RELATIVE_PATH = path.join("EdgePlus", "settings.json");
const FIRST_SWITCH = "switch-nativeSplit";
const OPEN_MILLISECONDS = 120_000;
const POLL_MILLISECONDS = 400;

function installedEdgeFolder() {
  const listing = execFileSync("reg", ["query", EDGE_APP_PATHS_KEY, "/ve"], { encoding: "utf8" });
  const match = /REG_SZ\s+(.+\.exe)\s*$/imu.exec(listing);
  if (match === null) {
    throw new Error("Edge is not installed: the sandboxed core reads its version from its folder");
  }

  return path.dirname(match[1].trim());
}

export interface RealCore {
  /** The pid of the window's process, which UI Automation finds the window by. */
  readonly windowPid: number;
  /** The settings file the core saved in its sandbox, as text; empty before the first save. */
  settings(): string;
  /** Ends the core and the window it started, by the pids it started. */
  stop(): void;
}

function findWindowPid(root: number) {
  const candidates = ownProcessTree(root).filter(pid => pid !== root);
  return candidates.find(pid =>
    exists({
      pid,
      id: FIRST_SWITCH
    })
  );
}

/** Builds nothing: run `cargo build` first. Resolves once the window shows its first switch. */
export async function startRealCore({ theme = "system" }: { theme?: string } = {}) {
  const data = mkdtempSync(path.join(tmpdir(), "edgeplus-test-"));
  const child: ChildProcess = spawn(CORE_EXE, [], {
    stdio: "ignore",
    env: {
      ...process.env,
      LOCALAPPDATA: data,
      EDGEPLUS_SIMULATE_EDGE_DIR: installedEdgeFolder(),
      EDGEPLUS_WINDOW_BACKGROUND: "1",
      EDGEPLUS_WINDOW_THEME: theme
    },
    windowsHide: true
  });
  const { pid } = child;
  if (pid === undefined) {
    throw new Error("the core did not start");
  }

  const deadline = Date.now() + OPEN_MILLISECONDS;
  let windowPid: number | undefined;
  while (windowPid === undefined && Date.now() < deadline) {
    windowPid = findWindowPid(pid);

    if (windowPid === undefined) {
      await sleep(POLL_MILLISECONDS);
    }
  }

  const core: RealCore = {
    windowPid: windowPid ?? 0,
    settings() {
      try {
        return readFileSync(path.join(data, SETTINGS_RELATIVE_PATH), "utf8");
      } catch {
        return "";
      }
    },
    stop() {
      spawnSync("taskkill", ["/PID", String(pid), "/T", "/F"]);
      rmSync(data, {
        recursive: true,
        force: true
      });
    }
  };
  if (windowPid === undefined) {
    core.stop();
    throw new Error("the window did not open");
  }

  return core;
}
