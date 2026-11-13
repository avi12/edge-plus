import { strict as assert } from "node:assert";
import { after, before, describe, it } from "node:test";
import { setTimeout as sleep } from "node:timers/promises";

import { ownProcessTree } from "./process-tree.ts";
import { type RealCore, startRealCore } from "./real-core.ts";
import { act, exists, names, pixel, read, toggles } from "./uia.ts";

// The panel window against the real core, on a desktop of its own
// (scripts/desktop/on-desktop.ps1) and in a sandbox (real-core.ts): it opens,
// shows every switch, and a flipped switch is saved by the core and shown
// again from what the core says. The window is driven only through UI
// Automation patterns and read back from what Windows drew. Run `cargo build`
// first.

const POLL_MILLISECONDS = 400;
const WAIT_MILLISECONDS = 30_000;
const LIGHT_LUMINANCE_GAP = 60;
const RED_END = 2;
const GREEN_END = 4;
const BLUE_END = 6;
const HEX = 16;
const BLUE_LEAD = 40;
const SWITCH_TRACK_ACROSS = "0.2";
const SWITCH_KEYS = [
  "nativeSplit",
  "livePreview",
  "theming",
  "cyberpunkSkin",
  "webUiFollowsTheme",
  "readingTheme",
  "enhancedFind",
  "protocolHandlers",
  "quietDevMode",
  "startWithWindows",
  "autoInject",
  "autoUpdate"
];

function until(description: string) {
  return async (check: () => boolean) => {
    const deadline = Date.now() + WAIT_MILLISECONDS;
    while (Date.now() < deadline) {
      if (check()) {
        return;
      }

      await sleep(POLL_MILLISECONDS);
    }

    throw new Error(`timed out waiting for ${description}`);
  };
}

function luminance(color: string) {
  const red = Number.parseInt(color.slice(0, RED_END), HEX);
  const green = Number.parseInt(color.slice(RED_END, GREEN_END), HEX);
  const blue = Number.parseInt(color.slice(GREEN_END, BLUE_END), HEX);
  return (red + green + blue) / (BLUE_END / 2);
}

function accentLead(color: string) {
  const red = Number.parseInt(color.slice(0, RED_END), HEX);
  const blue = Number.parseInt(color.slice(GREEN_END, BLUE_END), HEX);
  return blue - red;
}

describe("the panel window", () => {
  let core: RealCore | undefined;
  let pid = 0;
  let processTree: number[] = [];

  before(async () => {
    core = await startRealCore();
    pid = core.windowPid;
    processTree = ownProcessTree(pid);
  });

  after(() => {
    core?.stop();
    for (const processId of processTree) {
      assert.throws(() => process.kill(processId, 0), `process ${processId} is still alive`);
    }
  });

  it("renders every switch of the View", () => {
    const shown = toggles(pid);
    for (const key of SWITCH_KEYS) {
      const found = shown.find(entry => entry.id === `switch-${key}`);
      assert.ok(found, `a switch for ${key}`);
      assert.ok(found.enabled);
    }

    assert.equal(shown.length, SWITCH_KEYS.length);
    assert.ok(names(pid).includes("Split into any grid"));
  });

  it("draws the accent on a switch that is on", () => {
    const color = pixel({
      pid,
      id: "switch-nativeSplit",
      across: SWITCH_TRACK_ACROSS
    });
    assert.ok(accentLead(color) > BLUE_LEAD, `the switch is accent blue, not ${color}`);
  });

  it("saves a flipped switch in the core and shows the state the core answers with", async () => {
    const id = "switch-nativeSplit";
    const before = read({
      pid,
      id
    }).toggle;
    act({
      pid,
      id,
      action: "toggle"
    });
    await until("the switch to change")(
      () =>
        read({
          pid,
          id
        }).toggle !== before
    );
    await until("the core to save it")(
      () =>
        core?.settings().includes('"nativeSplit":false') === true ||
        core?.settings().includes('"nativeSplit": false') === true
    );
  });
});

describe("the panel window in each theme", () => {
  async function luminanceIn(theme: string) {
    const core = await startRealCore({ theme });
    try {
      await until("the theme picker")(() =>
        exists({
          pid: core.windowPid,
          id: "picker-light"
        })
      );
      await sleep(POLL_MILLISECONDS);
      return luminance(
        pixel({
          pid: core.windowPid,
          id: "store-link"
        })
      );
    } finally {
      core.stop();
    }
  }

  it("draws its controls light in the light theme and dark in the dark one", async () => {
    const light = await luminanceIn("light");
    const dark = await luminanceIn("dark");
    assert.ok(light - dark > LIGHT_LUMINANCE_GAP, `light ${light} against dark ${dark}`);
  });
});
