import { loadWinuiCatalogue } from "./catalogue.ts";
import { compileSheetModule, cssHash, SheetError } from "./sheet-module.ts";
import { compile } from "svelte/compiler";
import assert from "node:assert/strict";
import { test } from "node:test";

// Real Svelte compiles; the sheet must carry the class the elements get.

const resources = loadWinuiCatalogue();
const filename = "C:/app/ui/card.svelte";

const source = `<border class="card" class:big={true}>
  <text-block class="title">hi</text-block>
</border>

<style>
  .card { padding: 12px 16px; background: var(--card-background-fill-color-default) }
  text-block.title { font-weight: 600 }
  :global(:root) { --accent: #0af }
  @media (prefers-color-scheme: dark) { :global(:root) { --accent: #fa0 } }
</style>
`;

function moduleFor(text: string) {
  return compileSheetModule({
    filename,
    source: text,
    sheetId: "ui/card.svelte",
    resources,
    runtimeModule: "svelte-winui/css"
  });
}

test("the sheet's selectors carry the class the component's elements get", () => {
  const result = moduleFor(source);
  const component = compile(source, {
    filename,
    generate: "client",
    runes: true,
    cssHash
  });
  const scope = /svelte-[\w-]+/u.exec(component.js.code)?.[0];
  assert.ok(scope !== undefined);
  assert.deepEqual(result?.sheet.rules.map(rule => [rule.elementType, rule.classes]), [
    [null, ["card", scope]],
    ["text-block", ["title", scope]]
  ]);
  assert.match(result?.sheet.resourcesXaml ?? "", /AccentBrush/u);
});

test("the module registers the sheet, and accepts hot updates when asked", () => {
  const result = compileSheetModule({
    filename,
    source,
    sheetId: "ui/card.svelte",
    resources,
    runtimeModule: "svelte-winui/css",
    isHot: true
  });
  assert.match(result?.code ?? "", /^import \{ registerStyleSheet as registerWinuiSheet \} from "svelte-winui\/css";/u);
  assert.match(result?.code ?? "", /registerWinuiSheet\(\{"id":"ui\/card\.svelte","rules":/u);
  assert.match(result?.code ?? "", /import\.meta\.hot\.accept\(\)/u);
  assert.doesNotMatch(moduleFor(source)?.code ?? "", /import\.meta\.hot/u);
});

test("a component without a <style> has no sheet", () => {
  assert.equal(moduleFor("<border />"), null);
});

test("an error names the line of the .svelte file, not of the generated CSS", () => {
  const broken = `<border class="a" />\n\n<style>\n  .a {\n    color: red;\n    box-shadow: 0 0 2px red;\n  }\n</style>\n`;
  assert.throws(() => moduleFor(broken), (error: unknown) =>
    error instanceof SheetError && error.message.startsWith("ui/card.svelte:6:5: box-shadow has no WinUI meaning"));
});

test("a descendant selector Svelte keeps is reported at its rule", () => {
  const nested = `<border class="a"><text-block class="b" /></border>\n<style>\n  .a .b { color: red }\n</style>\n`;
  assert.throws(() => moduleFor(nested), (error: unknown) =>
    error instanceof SheetError && error.message.startsWith("ui/card.svelte:3:3:") && error.message.includes("descendant"));
});
