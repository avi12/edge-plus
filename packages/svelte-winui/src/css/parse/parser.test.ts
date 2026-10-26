import { BlockKind, CssSyntaxError, parseCss } from "./parser.ts";
import { createPositionMapper } from "./source-map.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

test("a rule keeps its selector, declarations and their positions", () => {
  const [block] = parseCss(".card { padding: 12px 16px;\n  color: red !important }");
  assert.equal(block?.kind, BlockKind.Rule);

  if (block?.kind !== BlockKind.Rule) {
    return;
  }

  assert.equal(block.selector, ".card");
  const declarations = block.declarations.map(
    declaration => [declaration.property, declaration.value, declaration.isImportant]
  );
  assert.deepEqual(declarations, [
    ["padding", "12px 16px", false],
    ["color", "red", true]
  ]);
  assert.deepEqual(block.declarations[1]?.at, {
    line: 2,
    column: 3
  });
});

test("comments, strings and parentheses do not end a declaration", () => {
  const [block] = parseCss("a { /* x; y */ font-family: \"A;B\", serif; background: url(a;b) }");
  assert.equal(block?.kind, BlockKind.Rule);

  if (block?.kind !== BlockKind.Rule) {
    return;
  }

  assert.deepEqual(block.declarations.map(declaration => declaration.value), ["\"A;B\", serif", "url(a;b)"]);
});

test("a custom property keeps its case and a property is lower-cased", () => {
  const [block] = parseCss(":root { --Accent: #0af; COLOR: red }");
  assert.equal(block?.kind, BlockKind.Rule);

  if (block?.kind !== BlockKind.Rule) {
    return;
  }

  assert.deepEqual(block.declarations.map(declaration => declaration.property), ["--Accent", "color"]);
});

test("@media holds rules and an at-rule ending in a semicolon has no children", () => {
  const blocks = parseCss("@import 'a.css'; @media (prefers-color-scheme: dark) { :root { --x: 1 } .a { color: red } }");
  const [importRule, media] = blocks;
  assert.equal(importRule?.kind, BlockKind.AtRule);
  assert.equal(media?.kind, BlockKind.AtRule);

  const areAtRules = importRule?.kind === BlockKind.AtRule && media?.kind === BlockKind.AtRule;
  if (!areAtRules) {
    return;
  }

  assert.equal(importRule.children, null);
  assert.equal(media.name, "media");
  assert.equal(media.prelude, "(prefers-color-scheme: dark)");
  assert.equal(media.children?.length, 2);
});

test("a rule nested in a rule is recorded where it starts", () => {
  const [block] = parseCss(".a {\n  color: red;\n  &:hover { color: blue }\n  margin: 0 }");
  assert.equal(block?.kind, BlockKind.Rule);

  if (block?.kind !== BlockKind.Rule) {
    return;
  }

  assert.deepEqual(block.nested, [{
    line: 3,
    column: 3
  }]);
  assert.deepEqual(block.declarations.map(declaration => declaration.property), ["color", "margin"]);
});

test("a stray closing brace is a syntax error with its position", () => {
  assert.throws(() => parseCss("a { color: red }\n}"), (error: Error) => error instanceof CssSyntaxError && error.position.line === 2);
});

test("a source map turns a generated position into the .svelte file's", () => {
  // Generated line 1 column 0 maps to source line 5 column 2 (AAIE: +0,+0,+4,+2)
  // and the segment after it, 10 columns on, to source column 12 (UAAU: +10,+0,+0,+10).
  const map = { mappings: "AAIE,UAAU" };
  const mapper = createPositionMapper(map);
  assert.deepEqual(
    mapper({
      line: 1,
      column: 1
    }), {
      line: 5,
      column: 3
    }
  );
  assert.deepEqual(
    mapper({
      line: 1,
      column: 11
    }), {
      line: 5,
      column: 13
    }
  );
});

test("without a map positions stay as they are", () => {
  const position = {
    line: 3,
    column: 4
  };
  assert.deepEqual(createPositionMapper(undefined)(position), position);
});
