import { spawnSync } from "node:child_process";
import path from "node:path";

import { z } from "zod";

// What a test does to the window the way a user does, through UI Automation
// (scripts/desktop/uia.ps1): the window is found by the pid the test started,
// a control by its AutomationId. No keys are sent and no mouse is moved.

const SCRIPT = path.join(import.meta.dirname, "..", "scripts", "desktop", "uia.ps1");
const UIA_TIMEOUT_MILLISECONDS = 60_000;

const stateSchema = z.object({
  name: z.string(),
  enabled: z.boolean(),
  toggle: z.string().optional(),
  value: z.string().optional()
});
const togglesSchema = z.array(
  z.object({
    id: z.string(),
    name: z.string(),
    toggle: z.string(),
    enabled: z.boolean()
  })
);
const listItemsSchema = z.array(
  z.object({
    name: z.string(),
    height: z.number(),
    inset: z.number()
  })
);
const pixelSchema = z.object({ color: z.string() });
const namesSchema = z.array(z.string());

interface UiaRequest {
  pid: number;
  action: string;
  id?: string;
  value?: string;
  item?: string;
}

function run({ pid, action, id = "", value = "", item = "" }: UiaRequest) {
  const result = spawnSync(
    "powershell.exe",
    [
      "-NoProfile",
      "-ExecutionPolicy",
      "Bypass",
      "-File",
      SCRIPT,
      "-ProcessId",
      String(pid),
      "-Action",
      action,
      "-Id",
      id,
      "-Value",
      value,
      "-Item",
      item
    ],
    {
      encoding: "utf8",
      timeout: UIA_TIMEOUT_MILLISECONDS
    }
  );
  const isFailed = result.status !== 0;
  if (isFailed) {
    throw new Error(`uia ${action} ${id} failed: ${result.stderr.trim() || result.stdout.trim()}`);
  }

  const output = result.stdout.trim();
  return output === "" ? [] : JSON.parse(output);
}

function asList<Item>(value: Item | Item[]) {
  return Array.isArray(value) ? value : [value];
}

/** Does `action` to a control (invoke, toggle, set-value, select-item) and returns what it reports after. */
export function act(request: UiaRequest) {
  return stateSchema.parse(run(request));
}

/** What a control reports now. */
export function read({ pid, id }: { pid: number; id: string }) {
  return act({
    pid,
    id,
    action: "read"
  });
}

/** Whether the window holds a control with this AutomationId. */
export function exists({ pid, id }: { pid: number; id: string }) {
  return (
    run({
      pid,
      id,
      action: "exists"
    }) === true
  );
}

/** Every name the window shows, in tree order. */
export function names(pid: number) {
  return namesSchema.parse(
    asList(
      run({
        pid,
        action: "names"
      })
    )
  );
}

/** Every toggle of the window: id, name, state and whether it is enabled. */
export function toggles(pid: number) {
  return togglesSchema.parse(
    asList(
      run({
        pid,
        action: "toggles"
      })
    )
  );
}

/** The color Windows drew in a control, as RRGGBB: at `across` of its width (the middle by default). */
export function pixel({ pid, id, across = "0.5" }: { pid: number; id: string; across?: string }) {
  return pixelSchema.parse(
    run({
      pid,
      id,
      value: across,
      action: "pixel"
    })
  ).color;
}

/** Every list item of the window: its name, its height and how far its content starts from its edge. */
export function listItems(pid: number) {
  return listItemsSchema.parse(
    asList(
      run({
        pid,
        action: "list-items"
      })
    )
  );
}
