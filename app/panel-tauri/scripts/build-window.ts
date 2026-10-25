import { build } from "esbuild";
import { cpSync, existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import path from "node:path";

// Builds dist/window, the window kit a published edgeplus.exe carries and
// unpacks: Node itself, the window's scripts bundled, dynwinrt's native
// module, the Windows App SDK bootstrapper and the runtime's packages, which
// the core installs for the user when they are missing.
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

function nugetCache(): string {
  const lock: unknown = JSON.parse(readFileSync(path.join(root, ".winapp", "winmds.lock.json"), "utf8"));
  const hasCache = typeof lock === "object" && lock !== null && "nuget_cache_dir" in lock;
  if (!hasCache || typeof lock.nuget_cache_dir !== "string") {
    throw new Error("Run pnpm restore first: .winapp/winmds.lock.json names no NuGet cache");
  }

  return lock.nuget_cache_dir;
}

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
  outExtension: { ".js": ".mjs" },
  bundle: true,
  platform: "node",
  format: "esm",
  target: "node24",
  minify: true,
  external: ["@microsoft/dynwinrt", "@microsoft/dynwinrt/*"],
  // The generated bindings are CommonJS; ESM output needs a require for them.
  banner: { js: "import { createRequire } from \"node:module\"; const require = createRequire(import.meta.url);" },
  logLevel: "warning",
  // The generated bindings declare some event removers twice (the later one
  // wins, as at run time); nothing of ours does.
  logOverride: { "duplicate-class-member": "silent" }
});
copy(process.execPath, path.join(kit, "node.exe"));
copy(path.join(dynwinrt, "package.json"), path.join(kit, "node_modules", "@microsoft", "dynwinrt", "package.json"));
for (const file of ["winrt.js", "index.js", "win32.js", "win32-unsafe.js", "win32-internal.js", "winrt-implementation.cjs", nativeModule]) {
  copy(path.join(dynwinrt, "dist", file), path.join(kit, "node_modules", "@microsoft", "dynwinrt", "dist", file));
}

copy(path.join(root, ".winapp", "bin", architecture, bootstrapName), path.join(kit, bootstrapName));
const runtime = path.join(nugetCache(), "microsoft.windowsappsdk.runtime", runtimeVersion, "tools", "MSIX", `win10-${architecture}`);
for (const name of runtimePackages) {
  copy(path.join(runtime, name), path.join(kit, "runtime", name));
}

process.stdout.write(`window kit: ${kit}\n`);
