import { type ChildProcess, spawn } from "node:child_process";
import { createServer, type Server } from "node:net";
import { createInterface } from "node:readline";

// Keeps the window's process up across the core. The core owns the window in
// production (its stdin closing ends it); in development the core restarts
// on every Rust change and the window must not, so this holds the window's
// stdin open itself and lends the link to whichever core is connected,
// through a named pipe and the relay (relay.ts). A core that leaves does not end the window, it keeps
// showing its last View until the next core connects and sends a fresh one.
// The window is started lazily, by the first core to connect, and again by
// the next one after the user closed it.

/** The core's end of the link, as the supervisor sees it. */
export interface CoreLink {
  /** One line from the window. */
  send(line: string): void;
  /** The window ended; the core learns it the way a core learns its child exited. */
  close(): void;
}

/** What a connected core holds. */
export interface CoreConnection {
  /** One line to the window. */
  write(line: string): void;
  /** The core is gone; the window stays. */
  disconnect(): void;
}

/** A started window: the process and the pipes of its link. */
export type WindowProcess = Pick<ChildProcess, "kill" | "once" | "pid" | "stdin" | "stdout">;

export interface WindowSupervisor {
  connect(link: CoreLink): CoreConnection;
  /** The pid of the window while one is up. */
  windowPid(): number | undefined;
  /** Ends the window, which only the supervisor's own end should do. */
  stop(): void;
}

/** Starts the window the way the core would: node on ui/main.ts, stdin and stdout piped. */
export function spawnWindowProcess({
  node,
  entry,
  environment
}: {
  node: string;
  entry: string;
  environment: NodeJS.ProcessEnv;
}): WindowProcess {
  return spawn(node, ["--disable-warning=ExperimentalWarning", entry], {
    stdio: ["pipe", "pipe", "inherit"],
    env: environment,
    windowsHide: true
  });
}

export function createWindowSupervisor({
  spawnWindow,
  log
}: {
  spawnWindow: () => WindowProcess;
  log: (line: string) => void;
}): WindowSupervisor {
  let windowProcess: WindowProcess | null = null;
  let current: CoreLink | null = null;

  function ensureWindow() {
    if (windowProcess !== null) {
      return;
    }

    const started = spawnWindow();
    windowProcess = started;
    log(`window: started (pid ${started.pid})`);
    // The window's stdin is never closed from here; a write to an ended
    // window is lost, not fatal.
    started.stdin?.on("error", () => undefined);

    if (started.stdout) {
      createInterface({ input: started.stdout })
        .on("line", line => current?.send(line))
        .on("error", () => undefined);
    }

    started.once("exit", code => {
      log(`window: ended (exit code ${code})`);
      windowProcess = null;
      current?.close();
      current = null;
    });
  }

  return {
    connect(link) {
      current = link;
      ensureWindow();
      return {
        write(line) {
          windowProcess?.stdin?.write(`${line}\n`);
        },
        disconnect() {
          const isCurrent = current === link;
          if (isCurrent) {
            current = null;
          }
        }
      };
    },
    windowPid: () => windowProcess?.pid,
    stop() {
      windowProcess?.kill();
    }
  };
}

/** Serves the supervisor on a named pipe; each connection is one core. */
export function servePipe({
  supervisor,
  pipeName
}: {
  supervisor: WindowSupervisor;
  pipeName: string;
}): Promise<Server> {
  const server = createServer(socket => {
    const connection = supervisor.connect({
      send: line => socket.write(`${line}\n`),
      close: () => socket.end()
    });
    // readline re-emits its input's errors on itself, and an error nobody listens to ends the process: a
    // core that went while the window was still sending is a lost line, not the end of the dev server.
    createInterface({ input: socket })
      .on("line", line => connection.write(line))
      .on("error", () => undefined);
    socket.on("error", () => undefined);
    socket.on("close", () => connection.disconnect());
  });
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(pipeName, () => resolve(server));
  });
}

/** The pipe a supervisor with this pid serves. */
export function pipeNameOf(pid: number) {
  return `\\\\.\\pipe\\edgeplus-dev-window-${pid}`;
}
