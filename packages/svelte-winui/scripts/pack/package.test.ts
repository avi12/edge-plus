import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import path from "node:path";
import { test } from "node:test";

// The source exports and what pnpm publishes (publishConfig) are two tables of
// one list of entry points: they must name the same entries, and a published
// entry must point at a file `pnpm build` makes.

const packageRoot = path.join(import.meta.dirname, "..", "..");

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function readPackage(): Record<string, unknown> {
  const parsed: unknown = JSON.parse(readFileSync(path.join(packageRoot, "package.json"), "utf8"));
  assert.ok(isRecord(parsed));
  return parsed;
}

function exportsOf(value: unknown): Record<string, unknown> {
  assert.ok(isRecord(value));
  return value;
}

function targetsOf(entry: unknown): string[] {
  if (typeof entry === "string") {
    return [entry];
  }

  return isRecord(entry) ? Object.values(entry).flatMap(targetsOf) : [];
}

const manifest = readPackage();
const sourceExports = exportsOf(manifest["exports"]);
const publishConfig = exportsOf(manifest["publishConfig"]);
const publishedExports = exportsOf(publishConfig["exports"]);

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
  const bin = exportsOf(publishConfig["bin"]);
  for (const target of targetsOf(bin)) {
    assert.ok(existsSync(path.join(packageRoot, target)), `bin points at ${target}`);
  }
});
