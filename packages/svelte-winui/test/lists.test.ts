import { ExampleProcess, isWindowsAppSdkRestored, numberOf } from "./example-process.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// The lists example (examples/lists) in a real WinUI window, off screen and
// never activated: a ListView of 1000 rows, each a Svelte component. Skipped
// where the Windows App SDK was not restored.

const timeoutMilliseconds = 300_000;
const rowCount = 1000;
// A 640 pixel window shows 16 rows; the list also realizes a few pages beyond
// its edges (ItemsStackPanel.CacheLength), about 76 in all.
const mostContainers = 150;

test("a list of 1000 rows keeps a screenful of rows, recycles them and ends none twice", {
  skip: !isWindowsAppSdkRestored,
  timeout: timeoutMilliseconds
}, async () => {
  const example = ExampleProcess.start("examples/lists/main.ts");
  try {
    const opened = await example.settled({ cmd: "stats" }, "stats");
    assert.equal(numberOf(opened, "rows"), rowCount);
    const shown = numberOf(opened, "live");
    assert.ok(shown > 5 && shown < mostContainers, `${shown} rows shown of ${rowCount}`);
    assert.equal(numberOf(opened, "destroyed"), 0);

    const scrolled = await example.settled({
      cmd: "scroll",
      key: "row-900"
    }, "scrolled");
    const afterScroll = numberOf(scrolled, "live");
    assert.ok(afterScroll < mostContainers, `${afterScroll} rows alive after scrolling to the end`);
    assert.ok(numberOf(scrolled, "destroyed") > 0, "no row was ended by the scroll");
    assert.equal(numberOf(scrolled, "created") - numberOf(scrolled, "destroyed"), afterScroll);

    const back = await example.settled({
      cmd: "scroll",
      key: "row-0"
    }, "scrolled");
    assert.ok(numberOf(back, "live") < mostContainers, "rows piled up scrolling back");

    // A rename reaches the row that shows it without making a new one.
    const before = await example.ask({ cmd: "stats" }, "stats");
    const renamed = await example.settled({
      cmd: "rename",
      key: "row-0",
      name: "Renamed"
    }, "renamed");
    assert.equal(numberOf(renamed, "created"), numberOf(before, "created"));
    assert.equal(numberOf(renamed, "destroyed"), numberOf(before, "destroyed"));

    // Alt+Down on the third row: the keyboard path, invoked through the list's handle.
    const moved = await example.settled({
      cmd: "move",
      key: "row-2",
      direction: "down"
    }, "moved");
    assert.deepEqual(moved["first"], ["row-0", "row-1", "row-3", "row-2", "row-4"]);
    assert.equal(numberOf(moved, "reorders"), 1);
    assert.equal(numberOf(moved, "rows"), rowCount);

    // A row that goes takes its component with it.
    const removed = await example.settled({
      cmd: "remove",
      key: "row-1"
    }, "removed");
    assert.equal(numberOf(removed, "rows"), rowCount - 1);
    assert.deepEqual(removed["first"], ["row-0", "row-3", "row-2", "row-4", "row-5"]);
    assert.ok(numberOf(removed, "live") < mostContainers);

    const cleared = await example.settled({ cmd: "clear" }, "cleared");
    assert.equal(numberOf(cleared, "rows"), 0);
    assert.equal(numberOf(cleared, "live"), 0, "rows outlived their items");

    example.send({ cmd: "close" });
    assert.equal(await example.exited, 0);
  } finally {
    example.kill();
  }
});
