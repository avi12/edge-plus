import assert from "node:assert/strict";
import { type ChildProcess, spawn } from "node:child_process";
import path from "node:path";
import { createInterface } from "node:readline";
import { after, describe, it } from "node:test";

import { type BuildResult, type CoreProcess, createCoreRunner } from "./core-runner.ts";
import { createWindowSupervisor, pipeNameOf, servePipe, type WindowProcess } from "./window-supervisor.ts";

// The dev server's moving parts without a window or a core: a stand-in window
// that answers every line with its own pid, so a line that survives the pipe
// and a window that stayed up both show in what comes back.

const RELAY_ENTRY = path.join(import.meta.dirname, "relay.ts");
const STAND_IN_WINDOW = `
  require("node:readline").createInterface({ input: process.stdin })
    .on("line", line => process.stdout.write(process.pid + ":" + line + "\\n"));
`;

function spawnStandInWindow(): WindowProcess {
  return spawn(process.execPath, ["-e", STAND_IN_WINDOW], { stdio: ["pipe", "pipe", "inherit"] });
}

function nextLine(child: ChildProcess) {
  return new Promise<string>(resolve => {
    const lines = createInterface({ input: child.stdout! });
    lines.once("line", line => {
      lines.close();
      resolve(line);
    });
  });
}

function startRelay(pipeName: string) {
  return spawn(process.execPath, ["--disable-warning=ExperimentalWarning", RELAY_ENTRY, pipeName], {
    stdio: ["pipe", "pipe", "inherit"]
  });
}

function exitOf(child: ChildProcess) {
  return new Promise<void>(resolve => child.once("exit", () => resolve()));
}

describe("the window supervisor", () => {
  const supervisor = createWindowSupervisor({
    spawnWindow: spawnStandInWindow,
    log: () => undefined
  });
  after(() => supervisor.stop());

  it("starts the window for the first core and carries lines both ways", async () => {
    const received: string[] = [];
    const first = supervisor.connect({
      send: line => received.push(line),
      close: () => undefined
    });
    first.write('{"id":1}');
    await new Promise(resolve => setTimeout(resolve, 300));
    assert.equal(received.length, 1);
    assert.equal(received[0], `${supervisor.windowPid()}:{"id":1}`);
    first.disconnect();
  });

  it("keeps the same window when a core leaves and the next one connects", async () => {
    const pid = supervisor.windowPid();
    assert.notEqual(pid, undefined);
    const received: string[] = [];
    const second = supervisor.connect({
      send: line => received.push(line),
      close: () => undefined
    });
    second.write("again");
    await new Promise(resolve => setTimeout(resolve, 300));
    assert.deepEqual(received, [`${pid}:again`]);
    assert.equal(supervisor.windowPid(), pid);
    second.disconnect();
  });

  it("tells the core when the window ends, and starts a new one for the next core", async () => {
    const pid = supervisor.windowPid();
    let hasClosed = false;
    supervisor.connect({
      send: () => undefined,
      close() {
        hasClosed = true;
      }
    });
    supervisor.stop();
    await new Promise(resolve => setTimeout(resolve, 500));
    assert.equal(hasClosed, true);
    assert.equal(supervisor.windowPid(), undefined);

    const received: string[] = [];
    const next = supervisor.connect({
      send: line => received.push(line),
      close: () => undefined
    });
    next.write("back");
    await new Promise(resolve => setTimeout(resolve, 300));
    assert.equal(received.length, 1);
    assert.notEqual(received[0], `${pid}:back`);
    assert.equal(received[0], `${supervisor.windowPid()}:back`);
  });
});

describe("the relay over the pipe", () => {
  const supervisor = createWindowSupervisor({
    spawnWindow: spawnStandInWindow,
    log: () => undefined
  });
  const pipeName = `${pipeNameOf(process.pid)}-test`;
  const server = servePipe({
    supervisor,
    pipeName
  });
  after(async () => {
    supervisor.stop();
    (await server).close();
  });

  it("passes JSON lines untouched, and the window outlives the core's relay", async () => {
    await server;
    const line = '{"id":7,"type":"view","data":{"text":"héllo \\" wörld","n":[1,2]}}';

    const firstRelay = startRelay(pipeName);
    firstRelay.stdin!.write(`${line}\n`);
    const firstAnswer = await nextLine(firstRelay);
    const windowPid = supervisor.windowPid();
    assert.equal(firstAnswer, `${windowPid}:${line}`);

    // The core leaves: its stdin closes, its relay ends, the window stays.
    const firstEnded = exitOf(firstRelay);
    firstRelay.stdin!.end();
    await firstEnded;
    assert.equal(supervisor.windowPid(), windowPid);

    // The next core connects to the same window.
    const secondRelay = startRelay(pipeName);
    secondRelay.stdin!.write("fresh view\n");
    assert.equal(await nextLine(secondRelay), `${windowPid}:fresh view`);
    const secondEnded = exitOf(secondRelay);
    secondRelay.stdin!.end();
    await secondEnded;
  });

  it("ends the core's relay when the window ends", async () => {
    await server;
    const relay = startRelay(pipeName);
    relay.stdin!.write("up\n");
    await nextLine(relay);
    const ended = exitOf(relay);
    supervisor.stop();
    await ended;
  });
});

describe("the core runner", () => {
  function fakeCore(name: string, events: string[]): CoreProcess {
    const { promise: exited, resolve: finish } = Promise.withResolvers<void>();
    return {
      pid: 1,
      exited,
      stop() {
        events.push(`stop ${name}`);
        finish();
        return exited;
      }
    };
  }

  function runner({ results, running }: { results: BuildResult[]; running?: number }) {
    const events: string[] = [];
    let started = 0;
    return {
      events,
      core: createCoreRunner({
        build() {
          events.push("build");
          return Promise.resolve(
            results.shift() ?? {
              isOk: true,
              output: ""
            }
          );
        },
        start() {
          started += 1;
          events.push(`start ${started}`);
          return fakeCore(String(started), events);
        },
        findRunning: () => running,
        log: line => events.push(`log ${line.split("\n")[0]}`)
      })
    };
  }

  it("replaces the core after a build that worked", async () => {
    const { events, core } = runner({
      results: [
        {
          isOk: true,
          output: ""
        },
        {
          isOk: true,
          output: ""
        }
      ]
    });
    await core.begin();
    await core.rebuild();
    assert.deepEqual(
      events.filter(event => !event.startsWith("log")),
      ["build", "start 1", "build", "stop 1", "start 2"]
    );
  });

  it("keeps the running core when a build fails, and says why", async () => {
    const { events, core } = runner({
      results: [
        {
          isOk: true,
          output: ""
        },
        {
          isOk: false,
          output: "error[E0425]: cannot find value"
        }
      ]
    });
    await core.begin();
    await core.rebuild();
    assert.equal(
      events.some(event => event.startsWith("stop")),
      false
    );
    assert.equal(events.at(-1), "log core: the build failed, keeping the running core");
  });

  it("refuses to start beside a core it did not start", async () => {
    const { events, core } = runner({
      results: [],
      running: 4242
    });
    await assert.rejects(core.begin(), /pid 4242/u);
    assert.deepEqual(events, []);
  });
});
