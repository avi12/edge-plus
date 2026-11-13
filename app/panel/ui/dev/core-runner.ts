// Keeps the real core current while the dev server stays up: a rebuild that
// succeeds replaces the running core (only the one this runner started), one
// that fails leaves the running core alone and says why. It never starts a
// core while another one is running, because the single-instance guard would
// hand the new one's start-up to the old one and the new one would exit.

/** A core this runner started. */
export interface CoreProcess {
  pid: number | undefined;
  /** Ends the core and resolves once it has ended. */
  stop(): Promise<void>;
  /** Resolves when the core ended, by itself or by `stop`. */
  exited: Promise<void>;
}

export interface BuildResult {
  isOk: boolean;
  /** The compiler's words, shown when the build failed. */
  output: string;
}

export interface CoreRunner {
  /** Builds and starts the first core; throws when another core is already running. */
  begin(): Promise<void>;
  /** Builds again and, if that worked, swaps the new core in; calls during a build coalesce into one more build. */
  rebuild(): Promise<void>;
  /** Ends the core this runner started. */
  stop(): Promise<void>;
}

export function createCoreRunner({
  build,
  start,
  findRunning,
  log
}: {
  build: () => Promise<BuildResult>;
  start: () => CoreProcess;
  /** The pid of a core running that this runner did not start, if any. */
  findRunning: () => number | undefined;
  log: (line: string) => void;
}) {
  let core: CoreProcess | null = null;
  let isBuilding = false;
  let isRebuildQueued = false;

  async function swapIn() {
    const old = core;
    core = null;
    await old?.stop();
    const started = start();
    core = started;
    log(`core: started (pid ${started.pid})`);
    started.exited
      .then(() => {
        const isStillCurrent = core === started;
        if (isStillCurrent) {
          core = null;
          log("core: ended; save a Rust file or press Enter to build and start it again");
        }
      })
      .catch(() => undefined);
  }

  async function rebuild() {
    if (isBuilding) {
      isRebuildQueued = true;
      return;
    }

    isBuilding = true;
    try {
      log("core: building");
      const result = await build();
      if (result.isOk) {
        await swapIn();
      } else {
        const keeping = core === null ? "no core is running" : "keeping the running core";
        log(`core: the build failed, ${keeping}\n${result.output}`);
      }
    } finally {
      isBuilding = false;
    }

    if (isRebuildQueued) {
      isRebuildQueued = false;
      await rebuild();
    }
  }

  return {
    async begin() {
      const other = findRunning();
      if (other !== undefined) {
        throw new Error(
          `edgeplus.exe is already running (pid ${other}); the single-instance guard would hand this start-up to it. Close it first.`
        );
      }

      await rebuild();
    },
    rebuild,
    async stop() {
      const old = core;
      core = null;
      await old?.stop();
    }
  } satisfies CoreRunner;
}
