import "../test-support/svelte-runes.ts";
import { createStdioTransport } from "./stdio.ts";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import path from "node:path";
import { test } from "node:test";

const { createHostChannel, HostError, hostState } = await import("../channel.svelte.ts");

const echoHost = path.join(import.meta.dirname, "..", "test-support", "echo-host.ts");

interface Counter {
  count: number;
}

test("state and actions round-trip with a host in a child process over stdio", async () => {
  const child = spawn(process.execPath, ["--disable-warning=ExperimentalWarning", echoHost], { stdio: ["pipe", "pipe", "pipe"] });
  const stderr: string[] = [];
  child.stderr.on("data", chunk => stderr.push(String(chunk)));
  const errors: Error[] = [];
  const channel = createHostChannel(createStdioTransport({
    input: child.stdout,
    output: child.stdin
  }), { onError: error => errors.push(error) });
  try {
    const counter = hostState<Counter>(channel, "counter");
    assert.deepEqual(await counter.ready, { count: 0 });

    await channel.send({ kind: "increment" });
    await channel.send({ kind: "increment" });
    assert.equal(counter.current?.count, 2, "patches arrive before their replies");

    await assert.rejects(channel.send({ kind: "fail" }), error => error instanceof HostError && error.message === "asked to fail");

    // Output of the host's own console.log went to stderr: stdout held only
    // protocol lines (anything else would have been reported as unreadable).
    await channel.send({ kind: "log" });
    assert.deepEqual(errors, []);
    assert.match(stderr.join(""), /noise that must not reach stdout/u);
  } finally {
    channel.dispose();
    child.kill();
  }
});

test("a line that is not JSON is reported as malformed, and later lines still arrive", async () => {
  const { PassThrough } = await import("node:stream");
  const input = new PassThrough();
  const output = new PassThrough();
  const seen: string[] = [];
  const stop = createStdioTransport({
    input,
    output
  }).listen({
    message: value => seen.push(`message:${JSON.stringify(value)}`),
    malformed: () => seen.push("malformed"),
    closed: () => seen.push("closed")
  });
  input.write("not json\n\n{\"a\":1}\n");
  input.end();
  await new Promise(resolve => setTimeout(resolve, 20));
  stop();
  assert.deepEqual(seen, ["malformed", "message:{\"a\":1}", "closed"]);
});
