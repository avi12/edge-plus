import type { BindingEntry } from "./binding-list.ts";
import { isBindingEntry, packageJsonWithBindings } from "./binding-list.ts";
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";

// `svelte-winui setup`: what an app needs on disk to use the package's
// controls, which are drawn from bindings the app generates for itself (the
// package imports them as #winui/*, mapped to the app's @winapp/bindings).
//
//   1. winapp.yaml, which names the Windows App SDK and the tools `winapp`
//      needs (kept if the app has one);
//   2. package.json: the classes the package uses in winapp.jsBindings, and
//      the @winapp/bindings dependency that links the generated folder;
//   3. `winapp restore` and `winapp node generate-bindings`, which fetch the
//      SDK, write .winapp (metadata, the bootstrapper) and generate the
//      bindings in .winapp/bindings.
//
// What it cannot do is link the generated folder into node_modules: the app's
// package manager does that, so it is run once more afterwards.

const bindingsDependency = "@winapp/bindings";
const bindingsLink = "link:.winapp/bindings";
const winappFile = "winapp.yaml";
const requiredBindingsUrl = new URL("../components/generated/required-bindings.json", import.meta.url);

// The Windows App SDK this package is built and tested against.
export const winappYaml = `# winapp restore reads this: the Windows App SDK the window is drawn with, and
# what restore itself needs (CppWinRT and the Windows SDK's metadata) to
# generate the bindings in .winapp/bindings from package.json's
# winapp.jsBindings.
packages:
  - name: Microsoft.Windows.CppWinRT
    version: 3.0.260520.1
  - name: Microsoft.WindowsAppSDK
    version: 2.2.0
  - name: Microsoft.Windows.ImplementationLibrary
    version: 1.0.260126.7
  - name: Microsoft.Windows.SDK.CPP
    version: 10.0.28000.2270
  - name: Microsoft.Windows.SDK.CPP.x64
    version: 10.0.28000.2270
`;

export interface SetupOptions {
  // The app's folder (the one with its package.json).
  directory: string;
  // The `winapp` command, when it is not in the app's node_modules/.bin or on
  // PATH.
  winapp?: string;
  // Write the files only; do not run winapp.
  isDryRun?: boolean;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

export function requiredBindings(): BindingEntry[] {
  const parsed: unknown = JSON.parse(readFileSync(requiredBindingsUrl, "utf8"));
  if (!Array.isArray(parsed)) {
    throw new TypeError("required-bindings.json is not a list");
  }

  return parsed.filter(isBindingEntry);
}

// package.json's text ready for the package's controls: the class list started
// if the app has none, filled with what the package needs, and the
// @winapp/bindings dependency present.
export function packageJsonForApp(original: string, required: BindingEntry[]): string {
  const parsed: unknown = JSON.parse(original);
  if (!isRecord(parsed)) {
    throw new TypeError("package.json is not an object");
  }

  const winapp = isRecord(parsed["winapp"]) ? parsed["winapp"] : {};
  const jsBindings = isRecord(winapp["jsBindings"]) ? winapp["jsBindings"] : {};
  const additionalWinmds = Array.isArray(jsBindings["additionalWinmds"]) ? jsBindings["additionalWinmds"] : [];
  const dependencies = isRecord(parsed["dependencies"]) ? parsed["dependencies"] : {};
  dependencies[bindingsDependency] ??= bindingsLink;
  const newline = original.includes("\r\n") ? "\r\n" : "\n";
  const started = {
    ...parsed,
    dependencies,
    winapp: {
      ...winapp,
      jsBindings: {
        ...jsBindings,
        additionalWinmds
      }
    }
  };
  const wanted = required.flatMap(entry => (entry.classes ?? []).map(name => ({
    namespace: entry.namespace,
    name
  })));
  return packageJsonWithBindings(JSON.stringify(started, null, 2).replaceAll("\n", newline), wanted);
}

// The app's own winapp, then PATH's.
function winappCommand(options: SetupOptions): string {
  if (options.winapp !== undefined) {
    return options.winapp;
  }

  const local = path.join(options.directory, "node_modules", ".bin", "winapp.cmd");
  return existsSync(local) ? local : "winapp";
}

function runWinapp(command: string, directory: string, args: string[]): void {
  process.stdout.write(`> winapp ${args.join(" ")}\n`);
  // A .cmd shim needs a shell on Windows.
  execFileSync(command, args, {
    cwd: directory,
    stdio: "inherit",
    shell: true
  });
}

export function setup(options: SetupOptions): void {
  const yamlPath = path.join(options.directory, winappFile);
  if (!existsSync(yamlPath)) {
    writeFileSync(yamlPath, winappYaml);
    process.stdout.write(`wrote ${winappFile}\n`);
  }

  const packagePath = path.join(options.directory, "package.json");
  writeFileSync(packagePath, packageJsonForApp(readFileSync(packagePath, "utf8"), requiredBindings()));
  process.stdout.write("package.json: winapp.jsBindings and the @winapp/bindings dependency are in place\n");

  if (options.isDryRun === true) {
    return;
  }

  const command = winappCommand(options);
  runWinapp(command, options.directory, ["restore"]);
  runWinapp(command, options.directory, ["node", "generate-bindings"]);
  process.stdout.write("done: run your package manager's install once more to link @winapp/bindings\n");
}
