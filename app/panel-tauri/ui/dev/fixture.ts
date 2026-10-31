import { startFakeCore } from "./fake-core.ts";

// Opens the window on the saved View with a stand-in core, for working on the
// window alone. EDGEPLUS_WINDOW_BACKGROUND=1 opens it off screen and without
// focus.
//
//   pnpm window

const core = startFakeCore({ isBackground: process.env["EDGEPLUS_WINDOW_BACKGROUND"] === "1" });
core.child.on("exit", code => {
  process.exit(code ?? 0);
});
