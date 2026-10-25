import { winuiCss } from "./vite.ts";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { test } from "node:test";

// The plugin's hooks called by hand, as Vite calls them.

const folder = mkdtempSync(path.join(tmpdir(), "winui-css-"));
const styled = path.join(folder, "styled.svelte");
const plain = path.join(folder, "plain.svelte");
const broken = path.join(folder, "broken.svelte");
writeFileSync(styled, "<border class=\"a\" />\n<style>\n  .a { padding: 4px }\n</style>\n");
writeFileSync(plain, "<border />\n");
writeFileSync(broken, "<border class=\"a\" />\n<style>\n  .a { float: left }\n</style>\n");

const plugin = winuiCss({ root: folder });
const warnings: string[] = [];
const context = {
  addWatchFile() {},
  warn: (message: string) => warnings.push(message),
  error(message: string): never {
    throw new Error(message);
  }
};

function run(name: "transform" | "resolveId" | "load", ...values: string[]): unknown {
  const found = plugin[name];
  if (typeof found !== "function") {
    throw new TypeError(`${name} is not a function`);
  }

  return Reflect.apply(found, context, values);
}

test("a component with a <style> imports its sheet module", () => {
  const result = run("transform", "compiled();", styled);
  assert.deepEqual(result, {
    code: `compiled();\nimport ${JSON.stringify(`${styled}?winui-css`)};\n`,
    map: null
  });
});

test("a component without one, and non-components, are left alone", () => {
  assert.equal(run("transform", "compiled();", plain), null);
  assert.equal(run("transform", "compiled();", `${styled}?svelte&type=style&lang.css`), null);
  assert.equal(run("transform", "x", path.join(folder, "a.ts")), null);
});

test("the sheet module is resolved and loaded as the translated sheet", () => {
  const id = `${styled}?winui-css`;
  assert.equal(run("resolveId", id), id);
  assert.equal(run("resolveId", "./other.js"), null);
  const code = String(run("load", id));
  assert.match(code, /registerStyleSheet\(sheet\)/u);
  assert.match(code, /"id":"styled.svelte"/u);
  assert.match(code, /import\.meta\.hot\.accept\(\)/u);
});

test("a sheet with an error fails the load with its line", () => {
  assert.throws(() => run("load", `${broken}?winui-css`), /^Error: broken\.svelte:3:8: float has no WinUI meaning/u);
});

test.after(() => {
  rmSync(folder, {
    recursive: true,
    force: true
  });
});
