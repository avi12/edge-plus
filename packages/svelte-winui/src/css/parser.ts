import type { Declaration, SourcePosition } from "./types.ts";

// The CSS Svelte emits for a component, read into rules and at-rules with the
// position of every declaration. Only what a stylesheet written for a
// component can hold: style rules, @media blocks, declarations and comments.
// Anything else (nesting, @import, @keyframes...) is reported by the caller
// as having no WinUI meaning, with the position this gives it.

export const BlockKind = {
  Rule: "rule",
  AtRule: "at-rule"
} as const;

export interface RuleBlock {
  kind: typeof BlockKind.Rule;
  selector: string;
  at: SourcePosition;
  declarations: Declaration[];
  // A rule nested in this one, which has no WinUI meaning.
  nested: SourcePosition[];
}

export interface AtRuleBlock {
  kind: typeof BlockKind.AtRule;
  name: string;
  prelude: string;
  at: SourcePosition;
  // Absent for an at-rule that ends with `;` (@import).
  children: Block[] | null;
}

export type Block = RuleBlock | AtRuleBlock;

export class CssSyntaxError extends Error {
  readonly at: SourcePosition;

  constructor(message: string, at: SourcePosition) {
    super(message);
    this.at = at;
  }
}

const quoteCharacters = new Set(["\"", "'"]);
const newline = "\n";

class Cursor {
  readonly text: string;
  index = 0;
  private readonly lineStarts: number[] = [0];

  constructor(text: string) {
    this.text = text;
    for (let i = 0; i < text.length; i++) {
      if (text[i] === newline) {
        this.lineStarts.push(i + 1);
      }
    }
  }

  get isAtEnd(): boolean {
    return this.index >= this.text.length;
  }

  positionAt(index: number): SourcePosition {
    let low = 0;
    let high = this.lineStarts.length - 1;
    while (low < high) {
      const middle = Math.ceil((low + high) / 2);
      if ((this.lineStarts[middle] ?? 0) <= index) {
        low = middle;
      } else {
        high = middle - 1;
      }
    }

    return {
      line: low + 1,
      column: index - (this.lineStarts[low] ?? 0) + 1
    };
  }

  skipComment(): boolean {
    const isComment = this.text.startsWith("/*", this.index);
    if (!isComment) {
      return false;
    }

    const end = this.text.indexOf("*/", this.index + 2);
    this.index = end === -1 ? this.text.length : end + 2;
    return true;
  }

  skipSpace(): void {
    while (!this.isAtEnd) {
      const character = this.text[this.index] ?? "";
      const isSpace = character.trim() === "";
      if (isSpace) {
        this.index++;
      } else if (!this.skipComment()) {
        return;
      }
    }
  }

  skipString(): void {
    const quote = this.text[this.index];
    this.index++;
    while (!this.isAtEnd && this.text[this.index] !== quote) {
      const isEscape = this.text[this.index] === "\\";
      this.index += isEscape ? 2 : 1;
    }

    this.index++;
  }
}

const Terminator = {
  OpenBrace: "{",
  CloseBrace: "}",
  Semicolon: ";",
  End: ""
} as const;
type Terminator = (typeof Terminator)[keyof typeof Terminator];

interface Prelude {
  text: string;
  at: SourcePosition;
  terminator: Terminator;
}

// Reads up to the next `{`, `;` or `}` outside parentheses and strings,
// without consuming the terminator.
function readPrelude(cursor: Cursor): Prelude {
  cursor.skipSpace();
  const start = cursor.index;
  const at = cursor.positionAt(start);
  let depth = 0;
  let text = "";
  while (!cursor.isAtEnd) {
    const character = cursor.text[cursor.index] ?? "";
    const isComment = cursor.text.startsWith("/*", cursor.index);
    if (isComment) {
      cursor.skipComment();
      continue;
    }

    if (quoteCharacters.has(character)) {
      const stringStart = cursor.index;
      cursor.skipString();
      text += cursor.text.slice(stringStart, cursor.index);
      continue;
    }

    if (character === "(") {
      depth++;
    } else if (character === ")") {
      depth--;
    }

    const isTerminator = depth <= 0
      && (character === Terminator.OpenBrace || character === Terminator.CloseBrace || character === Terminator.Semicolon);
    if (isTerminator) {
      return {
        text: text.trim(),
        at,
        terminator: character
      };
    }

    text += character;
    cursor.index++;
  }

  return {
    text: text.trim(),
    at,
    terminator: Terminator.End
  };
}

function toDeclaration(prelude: Prelude): Declaration {
  const colon = prelude.text.indexOf(":");
  if (colon === -1) {
    throw new CssSyntaxError(`expected "property: value", found "${prelude.text}"`, prelude.at);
  }

  const property = prelude.text.slice(0, colon).trim();
  const rawValue = prelude.text.slice(colon + 1).trim();
  const importantMatch = /\s*!important$/iu.exec(rawValue);
  const value = importantMatch === null ? rawValue : rawValue.slice(0, importantMatch.index);
  const isCustomProperty = property.startsWith("--");
  return {
    property: isCustomProperty ? property : property.toLowerCase(),
    value,
    isImportant: importantMatch !== null,
    at: prelude.at
  };
}

// After the `{` of a style rule.
function readRuleBody(cursor: Cursor, block: RuleBlock): void {
  while (!cursor.isAtEnd) {
    const prelude = readPrelude(cursor);
    cursor.index++;
    if (prelude.terminator === Terminator.OpenBrace) {
      // Nesting: skip its body and record where it was.
      block.nested.push(prelude.at);
      skipBody(cursor);
      continue;
    }

    const hasText = prelude.text !== "";
    if (hasText) {
      block.declarations.push(toDeclaration(prelude));
    }

    if (prelude.terminator === Terminator.CloseBrace) {
      return;
    }
  }
}

function skipBody(cursor: Cursor): void {
  let depth = 1;
  while (!cursor.isAtEnd && depth > 0) {
    const character = cursor.text[cursor.index] ?? "";
    if (cursor.text.startsWith("/*", cursor.index)) {
      cursor.skipComment();
      continue;
    }

    if (quoteCharacters.has(character)) {
      cursor.skipString();
      continue;
    }

    if (character === Terminator.OpenBrace) {
      depth++;
    } else if (character === Terminator.CloseBrace) {
      depth--;
    }

    cursor.index++;
  }
}

function readBlocks(cursor: Cursor, isNested: boolean): Block[] {
  const blocks: Block[] = [];
  while (!cursor.isAtEnd) {
    const prelude = readPrelude(cursor);
    const isClose = prelude.terminator === Terminator.CloseBrace;
    if (isClose) {
      if (isNested) {
        cursor.index++;
        return blocks;
      }

      throw new CssSyntaxError("unexpected }", prelude.at);
    }

    const isAtRule = prelude.text.startsWith("@");
    const isBlock = prelude.terminator === Terminator.OpenBrace;
    cursor.index++;

    if (isAtRule) {
      const [name = "", ...rest] = prelude.text.slice(1).split(/\s+/u);
      blocks.push({
        kind: BlockKind.AtRule,
        name: name.toLowerCase(),
        prelude: rest.join(" "),
        at: prelude.at,
        children: isBlock ? readBlocks(cursor, true) : null
      });
    } else if (isBlock) {
      const block: RuleBlock = {
        kind: BlockKind.Rule,
        selector: prelude.text,
        at: prelude.at,
        declarations: [],
        nested: []
      };
      readRuleBody(cursor, block);
      blocks.push(block);
    }
  }

  return blocks;
}

// Positions are in `css`: 1-based line and column.
export function parseCss(css: string): Block[] {
  return readBlocks(new Cursor(css), false);
}
