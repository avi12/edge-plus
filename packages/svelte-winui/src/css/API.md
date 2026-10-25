# svelte-winui CSS: the interface

A component's `<style>` block becomes WinUI Styles in two steps: the Vite
plugin translates it at build time (pure, no WinUI), the runtime applies it
(`native.ts` is the only part that touches WinUI).

## What the renderer calls (`src/css/index.ts`)

```ts
// Once, after Application.create has installed WinUI's own resources.
// `winui` is the package's WinUI import site (`import * as winui`): it needs
// Application, ResourceDictionary, Style, XamlReader, PropertyValue, projectAs,
// ColumnDefinition, RowDefinition, GridUnitType.
installCss(winui: CssWinui): void;

// For every element the renderer creates; `elementType` is the tag ("border",
// "text-block"). Skip it for an element that never gets class, style or style:.
styledElement(control: object, elementType: string): StyledElement;

interface StyledElement {
  setClasses(classNames: string): void;                        // the whole class attribute, on every change
  setInlineStyle(cssText: string | null): void;                // style="..."
  setStyleProperty(name: string, value: string | null): void;  // style:name={value}; null removes
  dispose(): void;                                             // when the element is removed
}
```

The renderer's `class` handler becomes `styled.setClasses(String(value))`, its
`style` handler `styled.setInlineStyle(...)`, and `style:` directives
`styled.setStyleProperty(...)`. A class no sheet knows applies nothing and
leaves the element alone (so a hand-applied Style survives); a throw means a
declaration the element's type cannot take, with the rule it came from.

Order does not matter between `installCss` and component modules: a sheet
registered before it is merged by it.

## What a component imports (`src/css/vite.ts`)

```ts
import { winuiCss } from "svelte-winui/css/vite";
import { cssHash } from "svelte-winui/css/sheet-module";

svelte({ compilerOptions: { cssHash, ...theRest } })   // the SAME options below
winuiCss({ root, compilerOptions: { cssHash, ...theRest }, runtimeModule: "svelte-winui/css" })
```

The plugin adds `import ".../Card.svelte?winui-css"` to each compiled
component that has a `<style>`. That module is

```ts
import { registerStyleSheet } from "<runtimeModule>";
registerStyleSheet(sheet);          // sheet = StyleSheetData as JSON
if (import.meta.hot) { import.meta.hot.accept(); }
```

`registerStyleSheet` with an id it has already seen replaces the sheet, keeps
its place in the cascade, and re-applies the classes of every live element.
`cssHash` is exported because Svelte's default scoping class depends on
`rootDir`; the component and its sheet must agree on it.

Without Vite (tests, the window kit), `compileSheetModule({ filename, source,
sheetId, resources, runtimeModule })` returns the module's code (or null for no
`<style>`); append it to the compiled component.

## The pure side (no WinUI, no Svelte)

```ts
translateStyleSheet({ file, css, map, resources }): { sheet: StyleSheetData, diagnostics }
formatDiagnostic(diagnostic): string   // "file:line:column: <property> has no WinUI meaning (<why>)"
```

`resources` is `loadWinuiCatalogue()` (node): the resource names in
`winui-resources.g.json`, exported from WinUI's generic.xaml by
`scripts/css/export-winui-resources.ts`.
