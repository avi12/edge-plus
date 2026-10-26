import { existsSync, readdirSync } from "node:fs";
import path from "node:path";

// Every file under the folder, at any depth; none when the folder is missing.
export function filesIn(folder: string): string[] {
  if (!existsSync(folder)) {
    return [];
  }

  return readdirSync(folder, {
    recursive: true,
    withFileTypes: true
  })
    .filter(entry => entry.isFile())
    .map(entry => path.join(entry.parentPath, entry.name));
}
