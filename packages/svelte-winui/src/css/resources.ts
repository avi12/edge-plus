// WinUI's resource names, and how a CSS `var(--name)` finds one.
//
//   var(--card-background-fill-color-default)  ->  CardBackgroundFillColorDefaultBrush
//
// The name is the custom property in Pascal case; where a property takes a
// brush, the `Brush` resource of that name is the one used (WinUI names a
// color `CardBackgroundFillColorDefault` and its brush the same plus `Brush`).
// Every name is checked against the list of resources WinUI really defines
// (winui-resources.g.json, exported from generic.xaml), with a suggestion
// when it is a typo.

export const ResourceKind = {
  Brush: "Brush",
  Color: "Color",
  Thickness: "Thickness",
  Double: "Double",
  CornerRadius: "CornerRadius",
  FontWeight: "FontWeight",
  FontFamily: "FontFamily",
  String: "String",
  Boolean: "Boolean",
  GridLength: "GridLength",
  // A resource that is another resource; its kind is its target's.
  Alias: "Alias",
  Style: "Style"
} as const;
export type ResourceKind = (typeof ResourceKind)[keyof typeof ResourceKind];

const brushSuffix = "Brush";
const maxSuggestions = 3;
const suggestionDistanceShare = 0.25;
const minSuggestionDistance = 3;

export function isResourceKind(value: string): value is ResourceKind {
  return Object.values<string>(ResourceKind).includes(value);
}

// An alias stands for another resource, whose kind is not known here.
function isUsableAs(candidate: ResourceKind | undefined, kind: ResourceKind): boolean {
  return candidate === kind || candidate === ResourceKind.Alias;
}

// "--card-background-fill-color-default" as "CardBackgroundFillColorDefault".
export function resourceNameOf(customProperty: string): string {
  return customProperty
    .replace(/^--/u, "")
    .split("-")
    .map(word => `${word.charAt(0).toUpperCase()}${word.slice(1)}`)
    .join("");
}

// "CardBackgroundFillColorDefault" as "--card-background-fill-color-default".
export function customPropertyOf(resourceName: string): string {
  const words = resourceName.replace(/([a-z0-9])([A-Z])/gu, "$1-$2").toLowerCase();
  return `--${words}`;
}

function editDistance(from: string, to: string): number {
  let previous = Array.from({ length: to.length + 1 }, (_, index) => index);
  for (let row = 1; row <= from.length; row++) {
    const current = [row];
    for (let column = 1; column <= to.length; column++) {
      const cost = from[row - 1] === to[column - 1] ? 0 : 1;
      current.push(
        Math.min(
          (previous[column] ?? 0) + 1,
          (current[column - 1] ?? 0) + 1,
          (previous[column - 1] ?? 0) + cost
        )
      );
    }

    previous = current;
  }

  return previous[to.length] ?? 0;
}

export interface ResourceCatalogue {
  kindOf(key: string): ResourceKind | undefined;
  // The key spelled as WinUI spells it, found by its lower-case form.
  keyOf(name: string): string | undefined;
  keysOfKind(kind: ResourceKind): string[];
  // The nearest names to a mistyped one, as `--custom-properties`.
  suggest(name: string, kind: ResourceKind): string[];
  // The same resources and some more, such as a stylesheet's own.
  extend(more: ReadonlyMap<string, ResourceKind>): ResourceCatalogue;
}

// `groups` maps a kind to the keys of that kind; `extra` adds resources a
// stylesheet defines itself.
export function createCatalogue(
  groups: Readonly<Record<string, readonly string[]>>,
  extra: ReadonlyMap<string, ResourceKind> = new Map()
): ResourceCatalogue {
  const kinds = new Map<string, ResourceKind>();
  for (const [kind, keys] of Object.entries(groups)) {
    if (isResourceKind(kind)) {
      for (const key of keys) {
        kinds.set(key, kind);
      }
    }
  }

  for (const [key, kind] of extra) {
    kinds.set(key, kind);
  }

  const byLowerCase = new Map([...kinds.keys()].map(key => [key.toLowerCase(), key]));
  return {
    extend: more => createCatalogue(groups, new Map([...extra, ...more])),
    kindOf: key => kinds.get(key),
    keyOf: name => byLowerCase.get(name.toLowerCase()),
    keysOfKind: kind => [...kinds].filter(([, candidate]) => isUsableAs(candidate, kind)).map(([key]) => key),
    suggest(name, kind) {
      const wanted = resourceNameOf(name).toLowerCase();
      const limit = Math.max(minSuggestionDistance, Math.round(wanted.length * suggestionDistanceShare));
      const isBrush = kind === ResourceKind.Brush;
      return [...kinds]
        .filter(([, candidate]) => isUsableAs(candidate, kind))
        .map(([key]) => {
          const shown = isBrush ? key.replace(/Brush$/u, "") : key;
          return [shown, editDistance(shown.toLowerCase(), wanted)] as const;
        })
        .filter(([, distance]) => distance <= limit)
        .sort((first, second) => first[1] - second[1])
        .slice(0, maxSuggestions)
        .map(([shown]) => customPropertyOf(shown));
    }
  };
}

export type VariableResolution =
  | {
    isFound: true;
    key: string;
  }
  | {
    isFound: false;
    reason: string;
  };

function describeSuggestions(suggestions: string[]): string {
  return suggestions.length === 0 ? "" : `; did you mean ${suggestions.join(" or ")}?`;
}

// The resource a `var(--name)` stands for where a value of `kind` is wanted.
export function resolveVariable(catalogue: ResourceCatalogue, name: string, kind: ResourceKind): VariableResolution {
  const base = resourceNameOf(name);
  const candidates = kind === ResourceKind.Brush ? [`${base}${brushSuffix}`, base] : [base];
  for (const candidate of candidates) {
    const key = catalogue.keyOf(candidate);
    const found = key === undefined ? undefined : catalogue.kindOf(key);
    const isUsable = found === kind || found === ResourceKind.Alias;
    const isResolved = key !== undefined && isUsable;
    if (isResolved) {
      return {
        isFound: true,
        key
      };
    }
  }

  const existing = candidates.map(candidate => catalogue.keyOf(candidate)).find(key => key !== undefined);
  const existingKind = existing === undefined ? undefined : catalogue.kindOf(existing);
  const isOtherKind = existing !== undefined && existingKind !== undefined;
  if (isOtherKind) {
    return {
      isFound: false,
      reason: `${name} is a WinUI ${existingKind} resource (${existing}), and a ${kind} is needed here`
    };
  }

  return {
    isFound: false,
    reason: `${name} is not a WinUI resource, nor a custom property this stylesheet declares${describeSuggestions(catalogue.suggest(name, kind))}`
  };
}

export function hasStyle(catalogue: ResourceCatalogue, key: string): boolean {
  const found = catalogue.keyOf(key);
  return found !== undefined && catalogue.kindOf(found) === ResourceKind.Style;
}
