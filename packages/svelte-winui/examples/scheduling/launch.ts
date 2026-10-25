import { startWindowsAppSdk } from "../bootstrap.ts";
import { Worker } from "node:worker_threads";

// The scheduling experiment (see README.md here): a background WinUI window
// whose Svelte state is changed from three kinds of callback, with no
// flushSync. Prints this process's pid so a script can find the window by pid.
//
//   node --disable-warning=ExperimentalWarning --import ../register-deps.ts launch.ts

const messageDelayMilliseconds = 2500;

startWindowsAppSdk();
process.stdout.write(`pid ${process.pid}\n`);

const worker = new Worker(new URL("./window.ts", import.meta.url));
// A message from the main thread, for the "host message" trigger.
setTimeout(() => worker.postMessage({ type: "bump" }), messageDelayMilliseconds);
worker.on("exit", code => process.exit(code));
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exit(1);
});
