import type { ResourceKind, VariableResolution } from "./resources.ts";
import type { Declaration, Severity, SourcePosition } from "./types.ts";
import {
  parseColor,
  parseLength,
  parseNumber,
  parsePercent,
  parseVariable,
  splitWords
} from "./values.ts";

// What the lowering of a declaration needs from its surroundings, and a reader
// that parses one declaration's value into WinUI's spelling, reporting what it
// cannot with the declaration's position.

export interface LowerOptions {
  // The element the style is for ("stack-panel"), or null when only the
  // syntax is being checked (a selector that names no element type).
  elementType: string | null;
  resolveVariable(declaration: Declaration, name: string, kind: ResourceKind): VariableResolution;
  hasStyle(key: string): boolean;
  report(severity: Severity, message: string, position: SourcePosition): void;
}

export const ThemeResourceMarkup = {
  open: "{ThemeResource ",
  close: "}"
} as const;

export function themeResource(key: string): string {
  return `${ThemeResourceMarkup.open}${key}${ThemeResourceMarkup.close}`;
}

export const NotANumber = "NaN";
export const Unbounded = "Infinity";

export class Reader {
  readonly declaration: Declaration;
  readonly options: LowerOptions;
  // Set once anything was reported as an error; the caller then emits nothing.
  hasFailed = false;

  constructor(declaration: Declaration, options: LowerOptions) {
    this.declaration = declaration;
    this.options = options;
  }

  get words(): string[] {
    return splitWords(this.declaration.value);
  }

  error(message: string): null {
    this.hasFailed = true;
    this.options.report("error", message, this.declaration.at);
    return null;
  }

  warn(message: string): void {
    this.options.report("warning", message, this.declaration.at);
  }

  // "padding: 12px 16 is not a length: expected ..."
  expected(what: string, found: string = this.declaration.value): null {
    return this.error(`${this.declaration.property}: expected ${what}, found "${found}"`);
  }

  // A `var(--name)` as the ThemeResource markup of a resource of `kind`.
  variable(token: string, kind: ResourceKind): string | null | undefined {
    const reference = parseVariable(token);
    if (reference === null) {
      return undefined;
    }

    const resolution = this.options.resolveVariable(this.declaration, reference.name, kind);
    if (resolution.isFound) {
      return themeResource(resolution.key);
    }

    return this.error(`${this.declaration.property}: ${resolution.reason}`);
  }

  brush(token: string): string | null {
    const fromVariable = this.variable(token, "Brush");
    if (fromVariable !== undefined) {
      return fromVariable;
    }

    const color = parseColor(token);
    return color ?? this.expected("a color (hex, rgb(), a name, transparent) or var(--brush)", token);
  }

  length(token: string): string | null {
    const fromVariable = this.variable(token, "Double");
    if (fromVariable !== undefined) {
      return fromVariable;
    }

    const pixels = parseLength(token);
    return pixels === null ? this.expected("a length in px (or rem, or 0)", token) : String(pixels);
  }

  // A length, or `auto` as NaN, WinUI's "not set".
  lengthOrAuto(token: string): string | null {
    return token.toLowerCase() === "auto" ? NotANumber : this.length(token);
  }

  pixels(token: string): number | null {
    const pixels = parseLength(token);
    return pixels ?? this.expected("a length in px (or rem, or 0)", token);
  }

  number(token: string): number | null {
    return parseNumber(token) ?? this.expected("a number", token);
  }

  // A number, or a percentage as its fraction.
  fraction(token: string): number | null {
    return parseNumber(token) ?? parsePercent(token) ?? this.expected("a number or a percentage", token);
  }

  // One of the keywords in `choices`, as the value it maps to.
  keyword<Value extends string>(token: string, choices: ReadonlyMap<string, Value>): Value | null {
    const found = choices.get(token.toLowerCase());
    return found ?? this.expected(`one of ${[...choices.keys()].join(", ")}`, token);
  }

  // The declaration's whole value must be exactly one token.
  single(): string | null {
    const [only, ...rest] = this.words;
    const isSingle = only !== undefined && rest.length === 0;
    return isSingle ? only : this.expected("a single value");
  }
}
