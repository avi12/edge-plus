import { loadWinuiCatalogue } from "./catalogue.ts";
import type { CssNative } from "./runtime.ts";
import { installCssRuntime, registerStyleSheet, styledElement, unregisterStyleSheet } from "./runtime.ts";
import { translateStyleSheet } from "./translate.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// The runtime against a fake WinUI: a "dictionary" is its XAML, a "style" is
// the XAML of the dictionary it was looked up in.

const resources = loadWinuiCatalogue();

interface FakeControl {
  style: string | null;
  styleChanges: number;
  columns: string | null;
  handlers: Map<string, () => void>;
  properties: Record<string, unknown>;
}

function control(properties: Record<string, unknown> = {}): FakeControl {
  return {
    style: null,
    styleChanges: 0,
    columns: null,
    handlers: new Map(),
    properties
  };
}

function isFakeControl(value: object): value is FakeControl {
  return "handlers" in value;
}

function fake(target: object): FakeControl {
  if (!isFakeControl(target)) {
    throw new TypeError("not a fake control");
  }

  return target;
}

const mergedResources: string[] = [];
const fakeNative: CssNative = {
  loadDictionary: xaml => ({ xaml }),
  mergeIntoApplication(dictionary) {
    mergedResources.push(String(Reflect.get(dictionary, "xaml")));
  },
  lookupStyle: dictionary => dictionary,
  setStyle(target, style) {
    const fakeControl = fake(target);
    fakeControl.style = style === null ? null : String(Reflect.get(style, "xaml"));
    fakeControl.styleChanges++;
  },
  setGridDefinitions(target, kind, lengths) {
    fake(target).columns = `${kind}:${lengths}`;
  },
  subscribe(target, event, handler) {
    const fakeControl = fake(target);
    fakeControl.handlers.set(event, handler);
    return () => fakeControl.handlers.delete(event);
  },
  read: (target, property) => fake(target).properties[property]
};

function sheet(id: string, css: string) {
  const translation = translateStyleSheet({
    file: id,
    css,
    resources
  });
  assert.deepEqual(translation.diagnostics, []);
  return translation.sheet;
}

// Sheets registered before the runtime is installed are merged by installing it.
registerStyleSheet(sheet("early.svelte", ":root { --accent: #0af }\n.early { opacity: 0.5 }"));
installCssRuntime(fakeNative);

test("a sheet registered before install has its custom properties merged at install", () => {
  assert.equal(mergedResources.length, 1);
  assert.match(mergedResources[0] ?? "", /<Color x:Key="Accent">#00AAFF<\/Color>/u);
});

test("classes become a Style, and a class toggled off removes it", () => {
  registerStyleSheet(sheet("card.svelte", ".card.svelte-1 { padding: 12px 16px }\n.on.svelte-1 { opacity: 0.5 }"));
  const target = control();
  const element = styledElement(target, "border");
  element.setClasses("card svelte-1");
  assert.match(target.style ?? "", /TargetType="Border"/u);
  assert.match(target.style ?? "", /<Setter Property="Padding" Value="16,12,16,12"\/>/u);
  assert.doesNotMatch(target.style ?? "", /Opacity/u);

  element.setClasses("card svelte-1 on");
  assert.match(target.style ?? "", /<Setter Property="Opacity" Value="0.5"\/>/u);

  element.setClasses("");
  assert.equal(target.style, null);
  element.dispose();
});

test("an element that matches nothing is left alone", () => {
  const target = control();
  const element = styledElement(target, "border");
  element.setClasses("nothing-styles-this");
  assert.equal(target.styleChanges, 0);
  element.dispose();
});

test("the same classes again do not rebuild the Style", () => {
  const target = control();
  const element = styledElement(target, "border");
  element.setClasses("card svelte-1");
  element.setClasses("svelte-1 card");
  assert.equal(target.styleChanges, 1);
  element.dispose();
});

test("a state rule listens for its events and restyles as the state changes", () => {
  registerStyleSheet(sheet("hover.svelte", ".btn.svelte-2 { opacity: 1 }\n.btn.svelte-2:hover { opacity: 0.5 }"));
  const target = control();
  const element = styledElement(target, "border");
  element.setClasses("btn svelte-2");
  assert.match(target.style ?? "", /Value="1"/u);
  assert.deepEqual([...target.handlers.keys()].sort(), ["PointerEntered", "PointerExited"]);

  target.handlers.get("PointerEntered")?.();
  assert.match(target.style ?? "", /Value="0.5"/u);
  target.handlers.get("PointerExited")?.();
  assert.match(target.style ?? "", /Value="1"/u);

  element.setClasses("other");
  assert.equal(target.handlers.size, 0);
  element.dispose();
});

test(":disabled and :checked start from the control's own state", () => {
  registerStyleSheet(sheet("flags.svelte", ".f.svelte-3:disabled { opacity: 0.3 }\n.f.svelte-3:checked { width: 9px }"));
  const target = control({
    isEnabled: false,
    isChecked: true
  });
  const element = styledElement(target, "border");
  element.setClasses("f svelte-3");
  assert.match(target.style ?? "", /Opacity/u);
  assert.match(target.style ?? "", /Width/u);
  element.dispose();
});

test("replacing a sheet (hot reload) restyles live elements and keeps the others", () => {
  registerStyleSheet(sheet("hot.svelte", ".h.svelte-4 { opacity: 0.1 }"));
  const target = control();
  const element = styledElement(target, "border");
  element.setClasses("h svelte-4");
  assert.match(target.style ?? "", /Value="0.1"/u);

  registerStyleSheet(sheet("hot.svelte", ".h.svelte-4 { opacity: 0.9 }"));
  assert.match(target.style ?? "", /Value="0.9"/u);

  unregisterStyleSheet("hot.svelte");
  assert.equal(target.style, null);
  element.dispose();
});

test("replacing a sheet leaves an element that matches none of its rules untouched", () => {
  registerStyleSheet(sheet("bystander.svelte", ".bystander { opacity: 0.3 }"));
  registerStyleSheet(sheet("other.svelte", ".other { opacity: 0.2 }"));
  const target = control();
  const element = styledElement(target, "border");
  element.setClasses("bystander");
  const changes = target.styleChanges;

  registerStyleSheet(sheet("other.svelte", ".other { opacity: 0.7 }"));
  assert.equal(target.styleChanges, changes);
  element.dispose();
});

test("a later sheet beats an earlier one at equal specificity", () => {
  registerStyleSheet(sheet("first.svelte", ".same { opacity: 0.1 }"));
  registerStyleSheet(sheet("second.svelte", ".same { opacity: 0.2 }"));
  const target = control();
  const element = styledElement(target, "border");
  element.setClasses("same");
  assert.match(target.style ?? "", /Value="0.2"/u);
  registerStyleSheet(sheet("first.svelte", ".same { opacity: 0.3 }"));
  assert.match(target.style ?? "", /Value="0.2"/u);
  element.dispose();
});

test("style= and style: beat every class", () => {
  const target = control();
  const element = styledElement(target, "border");
  element.setClasses("early");
  assert.match(target.style ?? "", /Value="0.5"/u);

  element.setInlineStyle("opacity: 0.25; padding: 4px");
  assert.match(target.style ?? "", /Value="0.25"/u);
  assert.match(target.style ?? "", /Value="4,4,4,4"/u);

  // style:opacity={0.75} reaches the renderer as a longer style string.
  element.setInlineStyle("opacity: 0.25; padding: 4px; opacity: 0.75");
  assert.match(target.style ?? "", /Value="0.75"/u);

  element.setInlineStyle(null);
  assert.match(target.style ?? "", /Value="0.5"/u);
  assert.doesNotMatch(target.style ?? "", /Padding/u);
  element.dispose();
});

test("a grid's templates are written to its definitions", () => {
  registerStyleSheet(sheet("grid.svelte", "grid.g { grid-template-columns: auto 1fr; gap: 4px }"));
  const target = control();
  const element = styledElement(target, "grid");
  element.setClasses("g");
  assert.equal(target.columns, "columns:Auto,*");
  assert.match(target.style ?? "", /RowSpacing/u);
  element.dispose();
});

test("a declaration the element cannot take fails with where it came from", () => {
  registerStyleSheet(sheet("bad.svelte", ".b { gap: 4px }"));
  const element = styledElement(control(), "border");
  assert.throws(() => element.setClasses("b"), /<border> cannot take the style of bad\.svelte#0: gap has no WinUI meaning on <border>/u);
  element.dispose();
});
