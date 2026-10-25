# svelte-winui CSS: the interface

A component's `<style>` block becomes WinUI Styles in two steps: the Vite
plugin translates it at build time (pure, no WinUI), the runtime applies it
(thin, the only part that touches WinUI).

## What the renderer calls (`src/css/runtime.ts`)

```ts
// Once, after Application.create has installed WinUI's own resources.
// `winui` is the package's WinUI import site (the module namespace).
installCss(winui: CssWinui): void;

// One call per element the renderer creates; `elementType` is the tag
// ("border", "text-block"). No call is needed for an element that is never
// given class, style or style:prop.
styledElement(control: object, elementType: string): StyledElement;

interface StyledElement {
  setClasses(classNames: string): void;            // the whole class attribute, on every change
  setInlineStyle(cssText: string | null): void;    // style="..."
  setStyleProperty(name: string, value: string | null): void; // style:name={value}; null removes
  dispose(): void;                                 // when the element is removed
}
```

The renderer's `class` handler becomes `styled.setClasses(String(value))`, its
`style` handler `styled.setInlineStyle(...)`, and `style:` directives
`styled.setStyleProperty(...)`. `class="Card"` lookups of hand-written XAML
Styles go away; a name no sheet knows applies nothing.

## What a component imports (`src/css/vite.ts`)

`winuiCss()` is a Vite plugin. For `Card.svelte` it emits the module
`Card.svelte?winui-css` and adds `import "./Card.svelte?winui-css"` to the
compiled component. That module calls

```ts
registerStyleSheet(sheet: StyleSheetData): void   // same sheet.id replaces: hot reload
unregisterStyleSheet(id: string): void
```

which merge the sheet's custom properties (`--name`) into the application's
resources and re-apply the classes of every live `StyledElement`.

## What the pure side exports (`src/css/index.ts`, no WinUI)

```ts
translateStyleSheet(input: { file: string; css: string; map?: SourceMapV3; resources: ResourceCatalogue }): Translation
// Translation = { sheet: StyleSheetData, diagnostics: Diagnostic[] }; an error diagnostic fails the build
formatDiagnostic(diagnostic): string   // "file:line:column: <property> has no WinUI meaning (<why>)"
```
