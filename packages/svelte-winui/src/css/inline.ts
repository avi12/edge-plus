import { BlockKind, parseCss } from "./parser.ts";
import type { LowerOptions } from "./reader.ts";
import { expandDeclaration } from "./shorthands.ts";
import type { Declaration, SourcePosition } from "./types.ts";

// `style="padding: 8px; opacity: .5"` and `style:padding={value}`: declarations
// that belong to one element and beat every rule of the stylesheet.

const inlinePosition: SourcePosition = {
  line: 1,
  column: 1
};

// Reads the text of a style attribute; a syntax problem throws.
export function parseInlineStyle(cssText: string, options: LowerOptions): Declaration[] {
  const [block] = parseCss(`x{${cssText}}`);
  if (block?.kind !== BlockKind.Rule) {
    return [];
  }

  return block.declarations.flatMap(declaration => expandDeclaration({
    ...declaration,
    at: inlinePosition
  }, options));
}

// One `style:name={value}` directive.
export function inlineDeclaration(name: string, value: string, options: LowerOptions): Declaration[] {
  return expandDeclaration({
    property: name.toLowerCase(),
    value,
    isImportant: false,
    at: inlinePosition
  }, options);
}
