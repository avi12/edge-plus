import { ownProcessTree } from "./process-tree.ts";
import {
  act,
  exists,
  listItems,
  names,
  pixel,
  read,
  toggles
} from "./uia.ts";
import { startFakeCore } from "#src/dev/fake-core.ts";
import { CoreMessageType } from "#src/protocol.g.ts";
import { strict as assert } from "node:assert";
import { after, before, describe, it } from "node:test";
import { setTimeout as sleep } from "node:timers/promises";

// The panel window against a stand-in core, on a desktop of its own
// (scripts/desktop/on-desktop.ps1): it opens, shows every switch, sends the
// core's actions and shows what the core answers. The window is driven only
// through UI Automation patterns and read back from what Windows drew.

const POLL_MILLISECONDS = 400;
const WAIT_MILLISECONDS = 30_000;
const OPEN_MILLISECONDS = 120_000;
const LIGHT_LUMINANCE_GAP = 60;
const RED_END = 2;
const GREEN_END = 4;
const BLUE_END = 6;
const HEX = 16;
const BLUE_LEAD = 40;
const MINIMUM_ROW_HEIGHT = 36;
const MAXIMUM_ROW_INSET = 8;
const SWITCH_TRACK_ACROSS = "0.2";
const SWITCH_KEYS = [
  "nativeSplit",
  "livePreview",
  "theming",
  "tabShape",
  "webUiFollowsTheme",
  "readingTheme",
  "enhancedFind",
  "protocolHandlers",
  "quietDevMode",
  "startWithWindows",
  "autoInject",
  "autoUpdate"
];
const LIGHT_THEME = "chkhiabninacddbgpadcfjkbmandlklp";

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

function pidOf(core: ReturnType<typeof startFakeCore>) {
  const { pid } = core.child;
  const isMissing = pid === undefined;
  if (isMissing) {
    throw new Error("the window process has no pid");
  }

  return pid;
}

type Core = ReturnType<typeof startFakeCore>;

function launched(options: Parameters<typeof startFakeCore>[0] = {}) {
  let started: Core | undefined;
  return {
    start() {
      started = startFakeCore(options);
      return started;
    },
    get core() {
      if (!started) {
        throw new Error("the window has not been started");
      }

      return started;
    }
  };
}

describe("the panel window", () => {
  const launch = launched();
  let pid = 0;
  let processTree: number[] = [];

  before(async () => {
    const core = launch.start();
    pid = pidOf(core);
    await Promise.race([
      core.waitFor("bounds"),
      sleep(OPEN_MILLISECONDS).then(() => {
        throw new Error("the window did not open");
      })
    ]);
    await until("the first switch")(() => exists({
      pid,
      id: "switch-nativeSplit"
    }));
    processTree = ownProcessTree(pid);
  });

  after(async () => {
    const code = await launch.core.stop();
    assert.equal(code, 0, "the window exits cleanly when the core closes it");
    for (const processId of processTree) {
      assert.throws(() => process.kill(processId, 0), `process ${processId} is still alive`);
    }
  });

  it("renders every switch of the View, in the state the View has", () => {
    const shown = toggles(pid);
    for (const key of SWITCH_KEYS) {
      const found = shown.find(entry => entry.id === `switch-${key}`);
      assert.ok(found, `a switch for ${key}`);
      assert.equal(found.toggle, "On");
      assert.ok(found.enabled);
    }

    assert.equal(shown.length, SWITCH_KEYS.length);
    assert.ok(names(pid).includes("Split into any grid"));
  });

  it("styles the theme rows through a scoped class: 36 high and no padding", () => {
    const rows = listItems(pid).filter(item => item.name.startsWith("theme-2077"));
    assert.equal(rows.length, 2);
    for (const row of rows) {
      assert.ok(row.height >= MINIMUM_ROW_HEIGHT, `row ${row.name} is ${row.height} high`);
      assert.ok(row.inset <= MAXIMUM_ROW_INSET, `row ${row.name} starts ${row.inset} in`);
    }
  });

  it("draws the accent on a switch that is on", () => {
    const color = pixel({
      pid,
      id: "switch-nativeSplit",
      across: SWITCH_TRACK_ACROSS
    });
    assert.ok(accentLead(color) > BLUE_LEAD, `the switch is accent blue, not ${color}`);
  });

  it("sends a flipped switch to the core and shows the View the core answers with", async () => {
    for (const key of SWITCH_KEYS) {
      const id = `switch-${key}`;
      act({
        pid,
        id,
        action: "toggle"
      });
      await until(`the core to hear ${key}`)(() => launch.core.actions.some(action => JSON.stringify(action) === JSON.stringify({
        kind: "setSwitch",
        key,
        isOn: false
      })));
      await until(`${key} to show off`)(() => read({
        pid,
        id
      }).toggle === "Off");
    }

    const card = launch.core.view.cards.find(candidate => candidate.key === "nativeSplit");
    assert.equal(card?.isOn, false, "the core applied the switch to its View");
  });

  it("follows the core when it changes the View on its own", async () => {
    for (const card of launch.core.view.cards) {
      card.isOn = true;
    }

    launch.core.pushView();
    await until("every switch to show on again")(() => toggles(pid).every(entry => entry.toggle === "On"));
  });

  it("shows the core's notice under the cards and its status line in the title bar", async () => {
    launch.core.request(CoreMessageType.notice, "The theme could not be read");
    launch.core.request(CoreMessageType.status, "Edge is up to date");
    await until("the notice and the status line")(() => names(pid).includes("The theme could not be read") && names(pid).includes("Edge is up to date"));
  });

  it("shows why a feature is held, and locks its switch", async () => {
    const card = launch.core.view.cards.find(candidate => candidate.key === "nativeSplit");
    assert.ok(card);
    card.hold = {
      kind: "paused",
      edge: {
        version: "156.0.1",
        isUpdated: true
      }
    };
    card.isLocked = true;
    launch.core.view.paused = ["nativeSplit"];
    launch.core.pushView();
    await until("the paused notice")(() => names(pid).some(name => name.includes("is paused until Edge+ supports this version")));
    assert.ok(names(pid).some(name => name.includes("Paused until Edge+ supports this version: Split into any grid")));
    assert.equal(
      read({
        pid,
        id: "switch-nativeSplit"
      }).enabled, false
    );
  });

  it("offers the update and the inject button as the View says, and sends their actions", async () => {
    assert.equal(
      exists({
        pid,
        id: "inject"
      }), false
    );
    launch.core.view.inject = "ready";
    launch.core.view.update = {
      version: "0.2.0",
      isInstaller: false
    };
    launch.core.pushView();
    await until("the inject card")(() => exists({
      pid,
      id: "inject"
    }));
    await until("the update card")(() => exists({
      pid,
      id: "restart-to-update"
    }));
    assert.ok(names(pid).includes("Edge+ 0.2.0 is ready"));
    act({
      pid,
      id: "inject",
      action: "invoke"
    });
    await until("the inject action")(() => launch.core.actions.some(action => JSON.stringify(action) === JSON.stringify({ kind: "inject" })));
    act({
      pid,
      id: "restart-to-update",
      action: "invoke"
    });
    await until("the restart action")(() => launch.core.actions.some(action => JSON.stringify(action) === JSON.stringify({ kind: "restartToUpdate" })));
  });

  it("adds a store theme through its form, and picks and removes themes", async () => {
    act({
      pid,
      id: "store-link",
      action: "set-value",
      value: "https://chromewebstore.google.com/detail/theme/abcdefghijklmnopabcdefghijklmnop"
    });
    act({
      pid,
      id: "store-add",
      action: "invoke"
    });
    await until("the store theme action")(() => launch.core.actions.some(action => JSON.stringify(action) === JSON.stringify({
      kind: "addStoreTheme",
      url: "https://chromewebstore.google.com/detail/theme/abcdefghijklmnopabcdefghijklmnop"
    })));
    act({
      pid,
      id: "picker-dark",
      action: "select-item",
      item: "theme-2077-light"
    });
    await until("the assign action")(() => launch.core.actions.some(action => JSON.stringify(action) === JSON.stringify({
      kind: "assignTheme",
      mode: "dark",
      id: LIGHT_THEME
    })));
    act({
      pid,
      id: `remove-${LIGHT_THEME}`,
      action: "invoke"
    });
    await until("the remove action")(() => launch.core.actions.some(action => JSON.stringify(action) === JSON.stringify({
      kind: "removeTheme",
      id: LIGHT_THEME
    })));
  });
});

describe("the panel window when the core refuses an action", () => {
  const launch = launched({ refuse: action => JSON.stringify(action).includes("quietDevMode") ? "Edge's Preferences is locked" : null });
  let pid = 0;

  before(async () => {
    const core = launch.start();
    pid = pidOf(core);
    await core.waitFor("bounds");
    await until("the switch")(() => exists({
      pid,
      id: "switch-quietDevMode"
    }));
  });

  after(async () => {
    await launch.core.stop();
  });

  it("shows the core's reason under the cards", async () => {
    act({
      pid,
      id: "switch-quietDevMode",
      action: "toggle"
    });
    await until("the error notice")(() => names(pid).some(name => name.includes("Edge's Preferences is locked")));
  });
});

describe("the panel window in each theme", () => {
  async function luminanceIn(theme: string) {
    const core = startFakeCore({ theme });
    try {
      const pid = pidOf(core);
      await core.waitFor("bounds");
      await until("the theme picker")(() => exists({
        pid,
        id: "picker-light"
      }));
      await sleep(POLL_MILLISECONDS);
      return luminance(
        pixel({
          pid,
          id: "store-link"
        })
      );
    } finally {
      await core.stop();
    }
  }

  it("draws its controls light in the light theme and dark in the dark one", async () => {
    const light = await luminanceIn("light");
    const dark = await luminanceIn("dark");
    assert.ok(light - dark > LIGHT_LUMINANCE_GAP, `light ${light} against dark ${dark}`);
  });
});
