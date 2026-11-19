import path from "node:path";

import { createStandaloneTray } from "winui-svelte/tray/standalone";

import tray from "./tray.ts";

// A dev run's tray, from the checkout's sources: a published kit starts the
// wrapper that `pnpm build:window` builds from tray.ts instead.
createStandaloneTray({
  ...tray,
  icon: path.join(import.meta.dirname, tray.icon)
});
