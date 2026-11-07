import { spawnSync } from "node:child_process";
import { z } from "zod";

const processesSchema = z.array(
  z.object({
    processId: z.number(),
    parentId: z.number()
  })
);

/** The pid and the pid of every process it started, by parent id, as they are now. */
export function ownProcessTree(root: number) {
  const listing = spawnSync("powershell.exe", [
    "-NoProfile",
    "-Command",
    "Get-CimInstance Win32_Process | Select-Object @{n='processId';e={$_.ProcessId}},@{n='parentId';e={$_.ParentProcessId}} | ConvertTo-Json -Compress"
  ], { encoding: "utf8" });
  const processes = processesSchema.parse(JSON.parse(listing.stdout));
  const tree = [root];
  for (const processId of tree) {
    for (const entry of processes) {
      const isChild = entry.parentId === processId && !tree.includes(entry.processId);
      if (isChild) {
        tree.push(entry.processId);
      }
    }
  }

  return tree;
}
