import { compileSvelte } from "./compile.ts";
import { compileDiagnostic, describeError, errorOf, formatDiagnostic } from "./diagnostics.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// A broken .svelte file is reported with its file, line and column, so the
// host's stderr says where to look rather than the window just ending.

const brokenSource = "<script>\n  let a = 1;\n</script>\n\n<div>\n  {#if a}\n</div>\n";
const brokenFile = "C:/app/bad.svelte";

function compileBroken(): Error | null {
  try {
    compileSvelte(brokenFile, brokenSource);
  } catch (error) {
    return errorOf(error);
  }

  return null;
}

test("a compile error names the file, line and column, with the code around it", () => {
  const error = compileBroken();
  assert.ok(error instanceof Error);
  const [header = "", ...frame] = error.message.split("\n");
  assert.match(header, /^C:\/app\/bad\.svelte:7:1: `<\/div>` attempted to close an element that was not open \[element_invalid_closing_tag\]$/u);
  assert.ok(frame.join("\n").includes("7: </div>"), "the code frame follows");
});

test("a diagnostic is read from Svelte's error, or from the Vite error that wraps it", () => {
  const error = compileBroken();
  assert.ok(error instanceof Error);
  assert.ok(error.cause instanceof Error);
  const direct = compileDiagnostic(error.cause);
  assert.equal(direct?.line, 7);
  assert.equal(direct?.column, 1);
  assert.equal(direct?.file, brokenFile);

  const wrapped = compileDiagnostic(new Error("transform failed", { cause: error.cause }));
  assert.deepEqual(wrapped, direct);
});

test("an error with no position is described as it is", () => {
  assert.equal(compileDiagnostic(new Error("plain")), null);
  assert.match(describeError(new Error("plain")), /^Error: plain/u);
  assert.equal(describeError("text"), "text");
});

test("a diagnostic without a frame is its header alone", () => {
  assert.equal(
    formatDiagnostic({
      file: "a.svelte",
      line: 2,
      column: 3,
      code: "",
      text: "oops",
      frame: ""
    }), "a.svelte:2:3: oops"
  );
});
