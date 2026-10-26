import { initWinappsdk } from "@microsoft/dynwinrt";
import { existsSync } from "node:fs";
import path from "node:path";

// The Windows App SDK (WinUI 3) comes first, before any WinRT object: its
// bootstrapper finds the runtime packages installed for the user.

const bootstrapVariable = "WINAPPSDK_BOOTSTRAP_DLL_PATH";
const bootstrapName = "Microsoft.WindowsAppRuntime.Bootstrap.dll";

export interface WindowsAppSdkOptions {
  // The runtime's version the app was built against.
  major: number;
  minor: number;
  // Where the bootstrapper may be, first match wins: beside a packaged app, or
  // in the folder `winapp restore` puts it in.
  folders: string[];
}

// The runtime is not installed (or not this version). The app installs the
// one it carries and starts again.
export class WindowsAppRuntimeMissing extends Error {}

function bootstrapPath(folders: string[]): string | undefined {
  return folders.map(folder => path.join(folder, bootstrapName)).find(candidate => existsSync(candidate));
}

export function initWindowsAppSdk(options: WindowsAppSdkOptions): void {
  const found = bootstrapPath(options.folders);
  if (found !== undefined) {
    process.env[bootstrapVariable] ??= found;
  }

  try {
    initWinappsdk(options.major, options.minor);
  } catch (error) {
    throw new WindowsAppRuntimeMissing(String(error), { cause: error });
  }
}
