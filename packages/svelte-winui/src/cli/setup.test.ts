import { mergedEntries } from "./binding-list.ts";
import { packageJsonForApp, requiredBindings, winappYaml } from "./setup.ts";
import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import path from "node:path";
import { test } from "node:test";

const required = [
  {
    namespace: "Microsoft.UI.Xaml",
    classes: ["Application", "Window"]
  },
  {
    namespace: "Microsoft.UI.Xaml.Controls",
    classes: ["Button", "StackPanel"]
  }
];

interface PackageJson {
  dependencies: Record<string, string>;
  winapp: {
    jsBindings: {
      additionalWinmds: Array<{
        namespace: string;
        classes?: string[];
      }>;
    };
  };
}

function isPackageJson(value: unknown): value is PackageJson {
  return typeof value === "object" && value !== null && "dependencies" in value && "winapp" in value;
}

function parsed(text: string): PackageJson {
  const value: unknown = JSON.parse(text);
  if (!isPackageJson(value)) {
    throw new TypeError("packageJsonForApp wrote no dependencies or winapp");
  }

  return value;
}

function packagesOf(text: string): string[] {
  return text.split(/\r?\n/u).filter(line => /^\s+(name|version):/u.test(line));
}

test("an app with no bindings list gets one, and the link to the generated folder", () => {
  const text = packageJsonForApp(
    JSON.stringify({
      name: "demo",
      dependencies: {
        svelte: "5.0.0"
      }
    }), required
  );
  const result = parsed(text);
  assert.equal(result.dependencies["@winapp/bindings"], "link:.winapp/bindings");
  assert.equal(result.dependencies["svelte"], "5.0.0");
  assert.deepEqual(result.winapp.jsBindings.additionalWinmds, required);
});

test("the classes an app already lists stay, and setup twice changes nothing", () => {
  const original = JSON.stringify({
    name: "demo",
    dependencies: {
      "@winapp/bindings": "link:elsewhere"
    },
    winapp: {
      jsBindings: {
        additionalWinmds: [
          {
            namespace: "Microsoft.UI.Xaml.Controls",
            classes: ["Slider"]
          },
          { namespace: "Windows.Storage" }
        ]
      }
    }
  });
  const once = packageJsonForApp(original, required);
  const result = parsed(once);
  assert.equal(result.dependencies["@winapp/bindings"], "link:elsewhere");
  const controls = result.winapp.jsBindings.additionalWinmds.find(entry => entry.namespace.endsWith(".Controls"));
  assert.deepEqual(controls?.classes, ["Button", "Slider", "StackPanel"]);
  assert.ok(result.winapp.jsBindings.additionalWinmds.some(entry => entry.namespace === "Windows.Storage"));
  assert.equal(packageJsonForApp(once, required), once);
});

test("the file keeps its line endings", () => {
  const crlf = packageJsonForApp("{\r\n  \"name\": \"demo\"\r\n}\r\n", required);
  assert.ok(crlf.includes("\r\n"));
  assert.ok(!/[^\r]\n/u.test(crlf));
});

test("the classes the package needs are listed, its own among them", () => {
  const listed = new Set(requiredBindings().flatMap(entry => (entry.classes ?? []).map(name => `${entry.namespace}.${name}`)));
  for (const name of ["Microsoft.UI.Xaml.Window", "Microsoft.UI.Dispatching.DispatcherQueue", "Microsoft.UI.Xaml.Controls.ToggleSwitch"]) {
    assert.ok(listed.has(name), `${name} is not in the required bindings`);
  }
});

// src/winui.ts is the package's one import site of the bindings; an app must
// generate every class it names. A class added there goes in `coreClasses` of
// scripts/codegen/tables.ts, and `pnpm gen:components` lists it.
test("every class src/winui.ts imports is among the required bindings", () => {
  const source = readFileSync(path.join(import.meta.dirname, "..", "winui.ts"), "utf8");
  const modules = [...source.matchAll(/"#winui\/([^"]+)"/gu)].map(match => match[1]?.toLowerCase());
  const listed = new Set(
    requiredBindings().flatMap(entry => (entry.classes ?? []).map(name => `${entry.namespace}.${name}`))
  );
  const listedModules = new Set([...listed].map(name => name.replaceAll(".", "/").toLowerCase()));
  const enums = new Set(["microsoft/ui/xaml/gridunittype"]);
  const missing = modules.filter(module => module !== undefined && !listedModules.has(module) && !enums.has(module));
  assert.deepEqual(missing, []);
});

test("mergedEntries sorts by namespace and keeps whole-namespace entries", () => {
  const merged = mergedEntries([{ namespace: "Windows.Storage" }], [
    {
      namespace: "Microsoft.UI.Xaml",
      name: "Window"
    }
  ]);
  assert.deepEqual(merged.map(entry => entry.namespace), ["Microsoft.UI.Xaml", "Windows.Storage"]);
});

// The panel's winapp.yaml is the one the package is built with.
test("the winapp.yaml setup writes names the SDK the panel is restored with", () => {
  const panel = path.join(import.meta.dirname, "..", "..", "..", "..", "app", "panel-tauri", "winapp.yaml");
  if (!existsSync(panel)) {
    return;
  }

  assert.deepEqual(packagesOf(winappYaml), packagesOf(readFileSync(panel, "utf8")));
});
