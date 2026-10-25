import { cssHash } from "../css/sheet-module.ts";
import { compileSvelte, rendererImport } from "./compile.ts";
import { svelteWinui, windowConfig, windowEnvironment, winuiPluginOptions } from "./plugin.ts";
import assert from "node:assert/strict";
import path from "node:path";
import { test } from "node:test";

// What the Vite plugin tells Svelte's own plugin, and the environment it sets
// up, without starting Vite.

test("components compile for the client against svelte-winui's renderer", () => {
  const { svelte, compile } = winuiPluginOptions();
  assert.equal(svelte.emitCss, false);
  assert.equal(svelte.compilerOptions?.css, "external");
  assert.deepEqual(compile, {
    generate: "client",
    cssHash,
    experimental: {
      customRenderer: "svelte-winui/host"
    }
  });
});

test("another renderer can be named", () => {
  const { compile } = winuiPluginOptions({ renderer: "./my-host.ts" });
  assert.equal(compile.experimental?.customRenderer, "./my-host.ts");
});

test("the plugin brings Svelte's own with its environment", () => {
  const names = svelteWinui().map(plugin => plugin.name);
  assert.ok(names.includes("svelte-winui:environment"));
  assert.ok(names.includes("svelte-winui:css"));
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
        noExternal: [/^svelte(?:\/|$)/u, "esm-env", "svelte-winui"]
      },
      dev: {
        moduleRunnerTransform: true
      }
    }
  });
});

test("a renderer given as a file is imported by relative path, a specifier as it is", () => {
  const component = path.join(import.meta.dirname, "a", "b.svelte");
  const renderer = path.join(import.meta.dirname, "c", "host.ts");
  assert.equal(rendererImport(component, renderer), "../c/host.ts");
  assert.equal(rendererImport(component, "svelte-winui/host"), "svelte-winui/host");
  assert.match(compileSvelte(component, "<div></div>"), /from 'svelte-winui\/host'/u);
});
