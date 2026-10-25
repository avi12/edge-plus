import assert from "node:assert/strict";
import { readdirSync, readFileSync } from "node:fs";
import { stripTypeScriptTypes } from "node:module";
import path from "node:path";
import { test } from "node:test";
import { compile, compileModule } from "svelte/compiler";
import { levels } from "./generated/metadata.ts";

// Every generated file goes through Svelte's compiler, with a custom renderer
// as the window has: a component that does not compile, or compiles with a
// warning, fails here and not when the window loads it.

const generated = path.join(import.meta.dirname, "generated");
const rendererPath = "../renderer.ts";

function filesIn(folder: string, suffix: string): string[] {
  return readdirSync(folder)
    .filter(name => name.endsWith(suffix) && !name.endsWith(".d.svelte.ts"))
    .map(name => path.join(folder, name));
}

test("every component compiles without a warning", () => {
  const files = filesIn(generated, ".svelte");
  assert.ok(files.length > 100, `only ${files.length} components`);
  for (const file of files) {
    const result = compile(readFileSync(file, "utf8"), {
      filename: file,
      generate: "client",
      runes: true,
      experimental: { customRenderer: rendererPath }
    });
    const warnings = result.warnings.map(warning => `${path.basename(file)}: ${warning.message}`);
    assert.deepEqual(warnings, []);
  }
});

test("every level's module compiles", () => {
  const files = filesIn(path.join(generated, "controls"), ".svelte.ts");
  assert.equal(files.length, Object.keys(levels).length);
  for (const file of files) {
    compileModule(stripTypeScriptTypes(readFileSync(file, "utf8")), {
      filename: file,
      generate: "client"
    });
  }
});

test("each component has a declaration and a level", () => {
  const components = Object.entries(levels).filter(([, level]) => level.isComponent);
  const declared = filesIn(generated, ".svelte").map(file => path.basename(file, ".svelte"));
  assert.deepEqual(declared.sort(), components.map(([name]) => name).sort());
});
