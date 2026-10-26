import path from "node:path";
import { initWindowsAppSdk } from "winui-svelte/bootstrap";

// Starts the Windows App SDK for an example's main thread, with the
// bootstrapper `winapp restore` put in this folder's .winapp. The examples run
// from the package's folder:
//
//   node --disable-warning=ExperimentalWarning examples/counter/launch.ts

const windowsAppSdkMajor = 2;
const windowsAppSdkMinor = 2;

export function startWindowsAppSdk(): void {
  initWindowsAppSdk({
    major: windowsAppSdkMajor,
    minor: windowsAppSdkMinor,
    folders: [path.join(process.cwd(), ".winapp", "bin", process.arch)]
  });
}
