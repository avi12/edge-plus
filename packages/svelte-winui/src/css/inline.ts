import { BlockKind, parseCss } from "./parser.ts";
import type { LowerOptions } from "./reader.ts";
import { expandDeclaration } from "./shorthands.ts";
import type { Declaration, SourcePosition } from "./types.ts";

// `style="padding: 8px; opacity: .5"`: declarations that belong to one element
// and beat every rule of the stylesheet. Svelte merges `style:padding={value}`
// directives into this one string before the renderer sees it.

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
