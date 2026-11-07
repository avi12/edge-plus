import {
  cpSync,
  existsSync,
  mkdirSync,
  readFileSync,
  renameSync,
  rmSync
} from "node:fs";
import path from "node:path";
import { buildApp } from "winui-svelte/vite";
import { z } from "zod";

// Builds dist/window, the window kit a published edgeplus.exe carries and
// unpacks: winui-svelte's buildApp writes the two bundles with dynwinrt, the
// C++ runtime it links, the Windows App SDK bootstrapper (bin/<arch>) and the
// theme resources beside them; this adds Node itself, names the bundles .mjs
// and adds the Windows App SDK runtime's packages, which the core installs for
// the user when they are missing.
//
//   pnpm build:window

const ROOT = path.join(import.meta.dirname, "..");
const KIT = path.join(ROOT, "dist", "window");
const BUNDLES = ["main", path.join("window", "worker")];
const ARCHITECTURE = "x64";
const RUNTIME_VERSION = "2.2.0";
const RUNTIME_PACKAGES = [
  "Microsoft.WindowsAppRuntime.2.msix",
  "Microsoft.WindowsAppRuntime.DDLM.2.msix",
  "Microsoft.WindowsAppRuntime.Main.2.msix",
  "Microsoft.WindowsAppRuntime.Singleton.2.msix"
];

const winmdsLockSchema = z.object({ nuget_cache_dir: z.string() });

function nugetCache() {
  const lock = winmdsLockSchema.safeParse(JSON.parse(readFileSync(path.join(ROOT, ".winapp", "winmds.lock.json"), "utf8")));
  if (!lock.success) {
    throw new Error("Run pnpm restore first: .winapp/winmds.lock.json names no NuGet cache");
  }

  return lock.data.nuget_cache_dir;
}

function copyRuntimePackage(name: string) {
  const from = path.join(nugetCache(), "microsoft.windowsappsdk.runtime", RUNTIME_VERSION, "tools", "MSIX", `win10-${ARCHITECTURE}`, name);
  if (!existsSync(from)) {
    throw new Error(`The window kit needs ${from}`);
  }

  const to = path.join(KIT, "runtime", name);
  mkdirSync(path.dirname(to), { recursive: true });
  cpSync(from, to);
}

rmSync(KIT, {
  recursive: true,
  force: true
});
await buildApp({
  root: ROOT,
  entries: {
    main: path.join(ROOT, "ui", "main.ts"),
    "window/worker": path.join(ROOT, "ui", "window", "worker.ts")
  },
  outDirectory: KIT,
  isMinified: true
});
for (const name of BUNDLES) {
  renameSync(path.join(KIT, `${name}.js`), path.join(KIT, `${name}.mjs`));
}

cpSync(process.execPath, path.join(KIT, "node.exe"));
for (const name of RUNTIME_PACKAGES) {
  copyRuntimePackage(name);
}

process.stdout.write(`window kit: ${KIT}\n`);
