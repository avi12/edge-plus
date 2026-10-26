import { ExampleProcess, isWindowsAppSdkRestored, numberOf } from "./example-process.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// The window example (examples/window) in a real WinUI window, off screen and
// never activated: <Window> with a custom title bar, each backdrop read back
// from WinUI, and its props followed live. Skipped where the Windows App SDK
// was not restored. (examples/window/run.ps1 captures each backdrop.)

const timeoutMilliseconds = 300_000;
const openedWidth = 520;
const openedHeight = 360;
const minimumWidth = 400;
const widenedTo = 600;

test("a window opens as its props say, follows them, and reports what happens to it", {
  skip: !isWindowsAppSdkRestored,
  timeout: timeoutMilliseconds
}, async () => {
  const example = ExampleProcess.start("examples/window/main.ts");
  try {
    const opened = await example.ask({ cmd: "info" }, "info");
    const scale = numberOf(opened, "scale");
    assert.equal(opened["title"], "Window example");
    assert.equal(opened["extendsIntoTitleBar"], true, "a title bar snippet extends the content into the title bar");
    assert.equal(opened["backdrop"], "acrylic");
    assert.equal(opened["isMaximizable"], false);
    assert.equal(numberOf(opened, "width"), Math.round(openedWidth * scale));
    assert.equal(numberOf(opened, "height"), Math.round(openedHeight * scale));
    assert.equal(numberOf(opened, "minimumWidth"), Math.round(minimumWidth * scale));
    assert.equal(numberOf(opened, "x"), -8000, "the placement is where it opened");
    assert.equal(numberOf(opened, "activations"), 0, "activate={false} opens without taking focus");

    for (const backdrop of ["mica", "micaAlt", "none", "acrylic"]) {
      const changed = await example.ask({
        cmd: "backdrop",
        value: backdrop
      }, "backdrop");
      assert.equal(changed["backdrop"], backdrop, `WinUI holds ${String(changed["backdrop"])}, not ${backdrop}`);
    }

    const retitled = await example.ask({
      cmd: "title",
      value: "Another title"
    }, "title");
    assert.equal(retitled["title"], "Another title");

    const widened = await example.ask({
      cmd: "width",
      value: widenedTo
    }, "width");
    assert.equal(numberOf(widened, "width"), Math.round(widenedTo * scale), "a new width prop resizes the window");
    const moved = await example.ask({
      cmd: "move",
      x: -7000,
      y: 50
    }, "moved");
    assert.equal(numberOf(moved, "x"), -7000);
    assert.ok(numberOf(moved, "boundsReports") >= 2, "onBoundsChanged heard the resize and the move");
    assert.deepEqual(moved["lastBounds"], {
      x: -7000,
      y: 50,
      width: Math.round(widenedTo * scale),
      height: Math.round(openedHeight * scale)
    });

    example.send({ cmd: "close" });
    assert.equal(await example.exited, 0, "closing the window ends the example");
  } finally {
    example.kill();
  }
});
