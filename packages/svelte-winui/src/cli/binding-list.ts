// The class list `winapp` generates bindings for (package.json's
// winapp.jsBindings.additionalWinmds), and how an app's list takes in the
// classes this package needs. Shared by the generator, which keeps the
// Edge+ panel's list current, and the `setup` command of an app that uses
// the package.

export interface BindingEntry {
  namespace: string;
  // Absent: every class of the namespace.
  classes?: string[];
}

export interface ClassName {
  namespace: string;
  name: string;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

export function isBindingEntry(value: unknown): value is BindingEntry {
  return isRecord(value) && typeof value["namespace"] === "string";
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
  const parsed: unknown = JSON.parse(original);
  const winapp = isRecord(parsed) ? parsed["winapp"] : undefined;
  const bindings = isRecord(winapp) ? winapp["jsBindings"] : undefined;
  const list = isRecord(bindings) ? bindings["additionalWinmds"] : undefined;
  const hasAdditionalWinmds = isRecord(bindings) && Array.isArray(list);
  if (!hasAdditionalWinmds) {
    throw new Error("package.json has no winapp.jsBindings.additionalWinmds");
  }

  bindings["additionalWinmds"] = mergedEntries(list.filter(isBindingEntry), wanted);
  return `${JSON.stringify(parsed, null, 2).replaceAll("\n", newline)}${newline}`;
}
