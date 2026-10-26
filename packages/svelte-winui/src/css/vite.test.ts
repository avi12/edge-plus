import type { NativeValue } from "../native-value.ts";
import { winuiCss } from "./vite.ts";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { test } from "node:test";

// The plugin's hooks called by hand, as Vite calls them.

const folder = mkdtempSync(path.join(tmpdir(), "winui-css-")).replaceAll("\\", "/");
const styled = `${folder}/styled.svelte`;
const plain = `${folder}/plain.svelte`;
const broken = `${folder}/broken.svelte`;
writeFileSync(styled, "<border class=\"a\" />\n<style>\n  .a { padding: 4px }\n</style>\n");
writeFileSync(plain, "<border />\n");
writeFileSync(broken, "<border class=\"a\" />\n<style>\n  .a { float: left }\n</style>\n");

const plugin = winuiCss({ root: folder });
const warnings: string[] = [];
const context = {
  addWatchFile() {},
  resolve: (source: string) => Promise.resolve({ id: source.replace("/rooted/", `${folder}/`) }),
  warn: (message: string) => warnings.push(message),
  error(message: string): never {
    throw new Error(message);
  }
};

type HookResult = NativeValue | void;

// A hook called with its own `this`, as Vite calls it, whatever stand-in the
// test gives it for Vite's context.
function callHook(hook: (...args: never[]) => HookResult, thisArg: object, args: NativeValue[]): HookResult {
  return Function.prototype.apply.call(hook, thisArg, args);
}

function run(name: "transform" | "resolveId" | "load", ...values: string[]) {
  const found = plugin[name];
  if (typeof found !== "function") {
    throw new TypeError(`${name} is not a function`);
  }

  return callHook(found, context, values);
}

test("a component with a <style> imports its sheet module", () => {
  const result = run("transform", "compiled();", styled);
  assert.deepEqual(result, {
    code: `compiled();\nimport ${JSON.stringify(`${styled}-winui-css.js`)};\n`,
    map: null
  });
});

test("a component without one, and non-components, are left alone", () => {
  assert.equal(run("transform", "compiled();", plain), null);
  assert.equal(run("transform", "compiled();", `${styled}?svelte&type=style&lang.css`), null);
  assert.equal(run("transform", "x", `${folder}/a.ts`), null);
});

test("the sheet module is resolved to the component's own path, and loaded as the translated sheet", async () => {
  const id = `${styled}-winui-css.js`;
  assert.equal(await run("resolveId", id), id);
  // Vite hands a root-relative import to resolve(), which finds the file.
  assert.equal(await run("resolveId", "/rooted/styled.svelte-winui-css.js"), id);
  assert.equal(await run("resolveId", "./other.js"), null);
  const code = String(run("load", id));
  assert.match(code, /registerWinuiSheet\(/u);
  assert.match(code, /"id":"styled.svelte"/u);
  assert.match(code, /import\.meta\.hot\.accept\(\)/u);
});

test("a change to a component reloads its sheet module too", () => {
  const sheet = { id: `${styled}-winui-css.js` };
  const environment = {
    moduleGraph: {
      getModuleById: (id: string) => (id === sheet.id ? sheet : undefined)
    }
  };
  const hotUpdate = plugin.hotUpdate;
  if (typeof hotUpdate !== "function") {
    throw new TypeError("hotUpdate is not a function");
  }

  const component = { id: styled };
  // A component with no sheet module is left to Vite's own reload.
  assert.deepEqual(
    callHook(hotUpdate, { environment }, [{
      file: `${folder}/new.svelte`,
      modules: [component]
    }]), undefined
  );
  // After the component was compiled, an edit to its <style> alone is the
  // sheet's: the component keeps running.
  run("transform", "compiled();", styled);
  assert.deepEqual(
    callHook(hotUpdate, { environment }, [{
      file: styled,
      modules: [component]
    }]), [sheet]
  );
  writeFileSync(styled, "<border class=\"a\" />\n<style>\n  .a { padding: 8px }\n</style>\n");
  assert.deepEqual(
    callHook(hotUpdate, { environment }, [{
      file: styled,
      modules: [component]
    }]), [sheet]
  );
  writeFileSync(styled, "<border class=\"b\" />\n<style>\n  .a { padding: 8px }\n</style>\n");
  assert.deepEqual(
    callHook(hotUpdate, { environment }, [{
      file: styled,
      modules: [component]
    }]), [component, sheet]
  );
  assert.equal(
    callHook(hotUpdate, { environment }, [{
      file: plain,
      modules: [component]
    }]), undefined
  );
});

test("a sheet with an error fails the load with its line", () => {
  assert.throws(() => run("load", `${broken}-winui-css.js`), /^Error: broken\.svelte:3:8: float has no WinUI meaning/u);
});

test.after(() => {
  rmSync(folder, {
    recursive: true,
    force: true
  });
});
