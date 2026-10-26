import type { Json } from "../messaging/json.ts";
import { readList, readRecord } from "./json-shape.ts";
import { z } from "zod";

// The class list `winapp` generates bindings for (package.json's
// winapp.jsBindings.additionalWinmds), and how an app's list takes in the
// classes this package needs. Shared by the generator, which keeps the
// Edge+ panel's list current, and the `setup` command of an app that uses
// the package.

const bindingEntrySchema = z.object({
  namespace: z.string(),
  // Absent: every class of the namespace.
  classes: z.array(z.string()).optional()
});

export type BindingEntry = z.infer<typeof bindingEntrySchema>;

export interface ClassName {
  namespace: string;
  name: string;
}

// The entries of a parsed list; what is not an entry is left out.
export function bindingEntriesOf(list: Json[]): BindingEntry[] {
  return list.flatMap(item => {
    const entry = bindingEntrySchema.safeParse(item);
    return entry.success ? [entry.data] : [];
  });
}

function compareText(left: string, right: string): number {
  return left.localeCompare(right);
}

// `existing` with the wanted classes added; the ones already there stay.
export function mergedEntries(existing: BindingEntry[], wanted: ClassName[]): BindingEntry[] {
  const byNamespace = new Map<string, Set<string>>();
  const wholeNamespaces: BindingEntry[] = [];
  for (const entry of existing) {
    if (entry.classes === undefined) {
      wholeNamespaces.push(entry);
    } else {
      byNamespace.set(entry.namespace, new Set([...byNamespace.get(entry.namespace) ?? [], ...entry.classes]));
    }
  }

  for (const type of wanted) {
    byNamespace.set(type.namespace, new Set([...byNamespace.get(type.namespace) ?? [], type.name]));
  }

  const entries = [...byNamespace.entries()].map(([namespace, classes]) => ({
    namespace,
    classes: [...classes].sort(compareText)
  }));
  return [...entries, ...wholeNamespaces].sort((left, right) => compareText(left.namespace, right.namespace));
}

// package.json's text with the wanted classes in winapp.jsBindings, in the
// file's own line endings. The list must exist (an app starts it); the rest
// of the file is kept as it is.
export function packageJsonWithBindings(original: string, wanted: ClassName[]): string {
  const newline = original.includes("\r\n") ? "\r\n" : "\n";
  const root = readRecord(JSON.parse(original));
  const winapp = readRecord(root?.["winapp"]);
  const bindings = readRecord(winapp?.["jsBindings"]);
  const list = readList(bindings?.["additionalWinmds"]);
  const hasAdditionalWinmds =
    root !== undefined && winapp !== undefined && bindings !== undefined && list !== undefined;
  if (!hasAdditionalWinmds) {
    throw new Error("package.json has no winapp.jsBindings.additionalWinmds");
  }

  // Each level is a copy with its keys in their order, so putting it back
  // keeps the file's layout.
  bindings["additionalWinmds"] = mergedEntries(bindingEntriesOf(list), wanted);
  winapp["jsBindings"] = bindings;
  root["winapp"] = winapp;
  return `${JSON.stringify(root, null, 2).replaceAll("\n", newline)}${newline}`;
}
