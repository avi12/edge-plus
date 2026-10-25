import { initWinappsdk } from "@microsoft/dynwinrt";
import path from "node:path";
import { pathToFileURL } from "node:url";

// Starts the Windows App SDK for an example's main thread, using the
// bootstrapper that `pnpm restore` put in Edge+'s panel (the examples borrow
// the panel's dependencies until the package has its own).

const windowsAppSdkMajor = 2;
const windowsAppSdkMinor = 2;
const bootstrapVariable = "WINAPPSDK_BOOTSTRAP_DLL_PATH";
const bootstrapName = "Microsoft.WindowsAppRuntime.Bootstrap.dll";
const panelFolder = path.join(import.meta.dirname, "..", "..", "..", "app", "panel-tauri");

export function startWindowsAppSdk(): void {
  process.env[bootstrapVariable] ??= path.join(panelFolder, ".winapp", "bin", process.arch, bootstrapName);
  initWinappsdk(windowsAppSdkMajor, windowsAppSdkMinor);
}

// A module of Edge+'s window, as a file URL a dynamic import accepts.
export function panelWindowModule(name: string): string {
  return pathToFileURL(path.join(panelFolder, "ui", "window", name)).href;
}
