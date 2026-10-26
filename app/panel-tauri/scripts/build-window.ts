import type { Plugin } from "esbuild";
import { build } from "esbuild";
import { execFileSync } from "node:child_process";
import {
  cpSync,
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync
} from "node:fs";
import path from "node:path";
import { compileSvelte } from "winui-svelte/compile";
import { z } from "zod";

// Builds dist/window, the window kit a published edgeplus.exe carries and
// unpacks: Node itself, the window's scripts bundled, dynwinrt's native
// module and the C++ runtime it links (absent from a clean Windows), the
// Windows App SDK bootstrapper and the runtime's packages, which the core
// installs for the user when they are missing.
//
//   pnpm build:window

const root = path.join(import.meta.dirname, "..");
const kit = path.join(root, "dist", "window");
const dynwinrt = path.join(root, "node_modules", "@microsoft", "dynwinrt");
const architecture = "x64";
const nativeModule = `dynwinrt.win32-${architecture}-msvc.node`;
const bootstrapName = "Microsoft.WindowsAppRuntime.Bootstrap.dll";
const runtimeVersion = "2.2.0";
const runtimePackages = [
  "Microsoft.WindowsAppRuntime.2.msix",
  "Microsoft.WindowsAppRuntime.DDLM.2.msix",
  "Microsoft.WindowsAppRuntime.Main.2.msix",
  "Microsoft.WindowsAppRuntime.Singleton.2.msix"
];

// dynwinrt's native module needs vcruntime140.dll, which only an installed
// Visual C++ Redistributable puts in System32. Node loads a native module with
// its own folder searched in place of node.exe's, so the copy (Visual Studio's
// redistributable one) goes beside the module.
const cppRuntime = "vcruntime140.dll";
const vswhere = path.join(process.env["ProgramFiles(x86)"] ?? "", "Microsoft Visual Studio", "Installer", "vswhere.exe");

function cppRuntimePath(): string {
  const installation = execFileSync(vswhere, ["-products", "*", "-latest", "-property", "installationPath"], { encoding: "utf8" }).trim();
  const redist = path.join(installation, "VC", "Redist", "MSVC");
  for (const version of readdirSync(redist).toSorted().toReversed()) {
    const folder = path.join(redist, version, architecture);
    const crt = existsSync(folder) ? readdirSync(folder).find(name => /^Microsoft\.VC\d+\.CRT$/v.test(name)) : undefined;
    if (crt !== undefined) {
      return path.join(folder, crt, cppRuntime);
    }
  }

  throw new Error(`No ${cppRuntime} under ${redist}: install Visual Studio's C++ workload`);
}

const winmdsLockSchema = z.object({ nuget_cache_dir: z.string() });

function nugetCache(): string {
  const lock = winmdsLockSchema.safeParse(JSON.parse(readFileSync(path.join(root, ".winapp", "winmds.lock.json"), "utf8")));
  if (!lock.success) {
    throw new Error("Run pnpm restore first: .winapp/winmds.lock.json names no NuGet cache");
  }

  return lock.data.nuget_cache_dir;
}

// The window's .svelte components, compiled as the bundle takes them in, each
// with the statements that register its <style> as WinUI Styles.
const svelteComponents: Plugin = {
  name: "svelte-components",
  setup(bundler) {
    // esbuild runs the filter in Go, which takes no regular expression flags.
    bundler.onLoad({ filter: /\.svelte(?:\.ts)?$/ }, ({ path: filename }) => ({
      contents: compileSvelte(filename, readFileSync(filename, "utf8"), { root: path.join(root, "ui") }),
      loader: "js"
    }));
  }
};

function copy(from: string, to: string): void {
  if (!existsSync(from)) {
    throw new Error(`The window kit needs ${from}`);
  }

  mkdirSync(path.dirname(to), { recursive: true });
  cpSync(from, to, { recursive: true });
}

rmSync(kit, {
  recursive: true,
  force: true
});
await build({
  entryPoints: {
    main: path.join(root, "ui", "main.ts"),
    "window/worker": path.join(root, "ui", "window", "worker.ts")
  },
  outdir: kit,
  outExtension: {
    ".js": ".mjs"
  },
  bundle: true,
  platform: "node",
  format: "esm",
  target: "node24",
  minify: true,
  external: ["@microsoft/dynwinrt", "@microsoft/dynwinrt/*"],
  // Svelte's browser build is the one that mounts components.
  conditions: ["browser"],
  plugins: [svelteComponents],
  // The generated bindings are CommonJS; ESM output needs a require for them.
  banner: {
    js: "import { createRequire } from \"node:module\"; const require = createRequire(import.meta.url);"
  },
  logLevel: "warning",
  // The generated bindings declare some event removers twice (the later one
  // wins, as at run time); nothing of ours does.
  logOverride: {
    "duplicate-class-member": "silent"
  }
});
copy(process.execPath, path.join(kit, "node.exe"));
copy(path.join(dynwinrt, "package.json"), path.join(kit, "node_modules", "@microsoft", "dynwinrt", "package.json"));
for (const file of ["winrt.js", "index.js", "win32.js", "win32-unsafe.js", "win32-internal.js", "winrt-implementation.cjs", nativeModule]) {
  copy(path.join(dynwinrt, "dist", file), path.join(kit, "node_modules", "@microsoft", "dynwinrt", "dist", file));
}

copy(cppRuntimePath(), path.join(kit, "node_modules", "@microsoft", "dynwinrt", "dist", cppRuntime));

copy(path.join(root, ".winapp", "bin", architecture, bootstrapName), path.join(kit, bootstrapName));
const runtime = path.join(nugetCache(), "microsoft.windowsappsdk.runtime", runtimeVersion, "tools", "MSIX", `win10-${architecture}`);
for (const name of runtimePackages) {
  copy(path.join(runtime, name), path.join(kit, "runtime", name));
}

process.stdout.write(`window kit: ${kit}\n`);
