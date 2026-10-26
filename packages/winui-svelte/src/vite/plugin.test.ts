import { cssHash } from "../css/runtime/sheet-module.ts";
import { compileSvelte, rendererImport } from "./compile.ts";
import { windowConfig, windowEnvironment, winuiPluginOptions, winuiSvelte } from "./plugin.ts";
import assert from "node:assert/strict";
import path from "node:path";
import { test } from "node:test";

// What the Vite plugin tells Svelte's own plugin, and the environment it sets
// up, without starting Vite.

test("components compile for the client against winui-svelte's renderer", () => {
  const { svelte, compile } = winuiPluginOptions();
  assert.equal(svelte.emitCss, false);
  assert.equal(svelte.compilerOptions?.css, "external");
  assert.deepEqual(compile, {
    generate: "client",
    cssHash,
    experimental: {
      customRenderer: "winui-svelte/host"
    }
  });
});

test("another renderer can be named", () => {
  const { compile } = winuiPluginOptions({ renderer: "./my-host.ts" });
  assert.equal(compile.experimental?.customRenderer, "./my-host.ts");
});

test("the plugin brings Svelte's own with its environment", () => {
  const names = winuiSvelte().map(plugin => plugin.name);
  assert.ok(names.includes("winui-svelte:environment"));
  assert.ok(names.includes("winui-svelte:css"));
  assert.ok(names.length > 1);
});

test("the window's module environment resolves Svelte's browser build", () => {
  const { resolve, environments } = windowConfig();
  assert.deepEqual(resolve, { conditions: ["browser"] });
  assert.deepEqual(environments, {
    [windowEnvironment]: {
      consumer: "server",
      resolve: {
        conditions: ["browser", "development"],
        noExternal: [/^svelte(?:\/|$)/u, "esm-env", "winui-svelte"]
      },
      dev: {
        moduleRunnerTransform: true
      }
    }
  });
});

test("a bundled component carries the statements that register its <style>", () => {
  const component = path.join(import.meta.dirname, "styled.svelte");
  const code = compileSvelte(component, "<border class=\"a\"></border>\n<style>\n  .a { padding: 4px }\n</style>\n");
  assert.match(code, /registerStyleSheet as registerWinuiSheet \} from "winui-svelte\/css"/u);
  assert.match(code, /"id":"styled\.svelte"/u);
});

test("a <style> WinUI cannot say fails the bundle with the rule's line", () => {
  const component = path.join(import.meta.dirname, "broken.svelte");
  assert.throws(
    () => compileSvelte(component, "<border class=\"a\"></border>\n<style>\n  .a { float: left }\n</style>\n"),
    /^Error: broken\.svelte:3:8: float has no WinUI meaning/u
  );
});

test("a renderer given as a file is imported by relative path, a specifier as it is", () => {
  const component = path.join(import.meta.dirname, "a", "b.svelte");
  const renderer = path.join(import.meta.dirname, "c", "host.ts");
  assert.equal(rendererImport(component, renderer), "../c/host.ts");
  assert.equal(rendererImport(component, "winui-svelte/host"), "winui-svelte/host");
  assert.match(compileSvelte(component, "<div></div>"), /from 'winui-svelte\/host'/u);
});
