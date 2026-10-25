import { PseudoState } from "./types.ts";
import { splitCommas } from "./values.ts";

// The selectors a component's stylesheet may use: a compound of an element
// type (kebab case, `text-block`), classes (`.a.b`), and at most one state.
// WinUI's Styles apply to one element by its own type and classes, so there is
// no way to say "inside" or "next to"; a combinator is an error that says so.
// Svelte's scoping class (`.svelte-abc123`) is an ordinary class here: the
// element carries it, which is what keeps one component's `.card` from
// styling another's.

export interface ParsedSelector {
  elementType: string | null;
  classes: string[];
  state: PseudoState | null;
  specificity: number;
  // `:root`, which declares custom properties and styles no element.
  isRoot: boolean;
}

export type SelectorResult =
  | {
    isValid: true;
    selector: ParsedSelector;
  }
  | {
    isValid: false;
    reason: string;
  };

const combinatorReason = "descendant, child and sibling selectors have no WinUI meaning (a style applies to an element by its own type and classes; give the element a class)";
const states = new Set<string>(Object.values(PseudoState));
const identifier = /^-?[a-z_][\w-]*/iu;
const classWeight = 1;
const typeWeight = 1;

class Compound {
  elementType: string | null = null;
  readonly classes: string[] = [];
  state: PseudoState | null = null;
  specificity = 0;
  isRoot = false;
}

function fail(reason: string): SelectorResult {
  return {
    isValid: false,
    reason
  };
}

// Reads one compound from `text`; `weight` is 0 inside :where().
function readCompound(text: string, compound: Compound, weight: number): string | null {
  let index = 0;
  while (index < text.length) {
    const rest = text.slice(index);
    const character = rest.charAt(0);
    const isStart = index === 0;
    const where = /^:where\(/iu.exec(rest);
    if (where !== null) {
      const end = matchingParenthesis(rest, where[0].length - 1);
      const inner = rest.slice(where[0].length, end);
      const problem = readCompound(inner, compound, 0);
      if (problem !== null) {
        return problem;
      }

      index += end + 1;
      continue;
    }

    const isPseudo = character === ":" && !rest.startsWith("::");
    const isUniversal = character === "*" && isStart;
    const isTypeSelector = isStart && identifier.test(rest);
    if (character === ".") {
      const name = identifier.exec(rest.slice(1))?.[0];
      if (name === undefined) {
        return `"${rest}" is not a class name`;
      }

      compound.classes.push(name);
      compound.specificity += weight * classWeight;
      index += 1 + name.length;
    } else if (isPseudo) {
      const name = identifier.exec(rest.slice(1))?.[0]?.toLowerCase();
      const problem = readPseudo(compound, name, weight);
      if (problem !== null) {
        return problem;
      }

      index += 1 + (name?.length ?? 0);
    } else if (isUniversal) {
      index++;
    } else if (isTypeSelector) {
      const name = identifier.exec(rest)?.[0] ?? "";
      compound.elementType = name.toLowerCase();
      compound.specificity += weight * typeWeight;
      index += name.length;
    } else {
      return unsupportedCharacter(character, rest);
    }
  }

  return null;
}

function matchingParenthesis(text: string, open: number): number {
  let depth = 0;
  for (let index = open; index < text.length; index++) {
    depth += text[index] === "(" ? 1 : 0;
    depth -= text[index] === ")" ? 1 : 0;

    if (depth === 0) {
      return index;
    }
  }

  return text.length;
}

function readPseudo(compound: Compound, name: string | undefined, weight: number): string | null {
  if (name === "root") {
    compound.isRoot = true;
    return null;
  }

  const isKnownState = name !== undefined && states.has(name);
  if (!isKnownState) {
    const known = [...states].map(state => `:${state}`).join(", ");
    return `:${name ?? ""} has no WinUI meaning (a control reports ${known})`;
  }

  if (compound.state !== null) {
    return "only one state (:hover, :focus...) can be on a selector";
  }

  compound.state = stateOf(name);
  compound.specificity += weight * classWeight;
  return null;
}

function stateOf(name: string): PseudoState | null {
  return Object.values(PseudoState).find(state => state === name) ?? null;
}

function unsupportedCharacter(character: string, rest: string): string | null {
  const isCombinator = character === " " || character === ">" || character === "+" || character === "~";
  if (isCombinator) {
    return combinatorReason;
  }

  if (character === "[") {
    return "attribute selectors have no WinUI meaning (use a class)";
  }

  if (character === "#") {
    return "id selectors have no WinUI meaning (use a class)";
  }

  return `"${rest}" has no WinUI meaning in a selector`;
}

// A comma-separated list, each of which becomes a rule.
export function parseSelectorList(text: string): SelectorResult[] {
  return splitCommas(text).map(selector => {
    const compound = new Compound();
    const problem = readCompound(selector.trim(), compound, 1);
    if (problem !== null) {
      return fail(problem);
    }

    const hasMore = compound.classes.length > 0 || compound.elementType !== null || compound.state !== null;
    const isRootWithMore = compound.isRoot && hasMore;
    if (isRootWithMore) {
      return fail(":root cannot be combined with a class, type or state");
    }

    return {
      isValid: true,
      selector: {
        elementType: compound.elementType,
        classes: compound.classes,
        state: compound.state,
        specificity: compound.specificity,
        isRoot: compound.isRoot
      }
    };
  });
}
