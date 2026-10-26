import { jsonSchema } from "../../src/messaging/json.ts";
import type { Json } from "../../src/messaging/json.ts";
import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import path from "node:path";
import { test } from "node:test";
import { z } from "zod";

// The source exports and what pnpm publishes (publishConfig) are two tables of
// one list of entry points: they must name the same entries, and a published
// entry must point at a file `pnpm build` makes.

const packageRoot = path.join(import.meta.dirname, "..", "..");

const exportsSchema = z.record(z.string(), jsonSchema);

const manifestSchema = z.object({
  exports: exportsSchema,
  publishConfig: z.object({
    exports: exportsSchema,
    bin: z.record(z.string(), z.string())
  })
});

function targetsOf(entry: Json): string[] {
  if (typeof entry === "string") {
    return [entry];
  }

  const nested = exportsSchema.safeParse(entry);
  return nested.success ? Object.values(nested.data).flatMap(targetsOf) : [];
}

const manifest = manifestSchema.parse(JSON.parse(readFileSync(path.join(packageRoot, "package.json"), "utf8")));
const sourceExports = manifest.exports;
const publishedExports = manifest.publishConfig.exports;

test("the published exports name the same entries as the source exports", () => {
  assert.deepEqual(Object.keys(publishedExports).sort(), Object.keys(sourceExports).sort());
});

test("every source export is a file of the repository", () => {
  for (const [name, entry] of Object.entries(sourceExports)) {
    for (const target of targetsOf(entry)) {
      assert.ok(existsSync(path.join(packageRoot, target)), `${name} points at ${target}, which is missing`);
    }
  }
});

test("every published export is a file of dist once it is built", { skip: !existsSync(path.join(packageRoot, "dist")) }, () => {
  for (const [name, entry] of Object.entries(publishedExports)) {
    for (const target of targetsOf(entry).filter(file => file.startsWith("./dist/"))) {
      assert.ok(existsSync(path.join(packageRoot, target)), `${name} points at ${target}, which pnpm build did not make`);
    }
  }
});

test("the published bin is a file of dist once it is built", { skip: !existsSync(path.join(packageRoot, "dist")) }, () => {
  for (const target of Object.values(manifest.publishConfig.bin)) {
    assert.ok(existsSync(path.join(packageRoot, target)), `bin points at ${target}`);
  }
});
