import type { KeyedItems } from "./keyed-list.ts";
import { syncKeyed } from "./keyed-list.ts";
import {
  autoLength,
  boxed,
  pixelLength,
  starLength,
  thickness
} from "./layout.ts";
import type { GridLength } from "./markup.ts";
import {
  camelCase,
  gridLength,
  GridUnit,
  isNumeric,
  pascalCase,
  splitList,
  thicknessSides
} from "./markup.ts";
import { style, StyleKey } from "./styles.ts";
import type { ElementNode, FragmentNode, NativeHost, TreeNode } from "./svelte-tree.ts";
import { createTreeRenderer, NodeType, parentElement } from "./svelte-tree.ts";
import type { FrameworkElement, UIElement } from "./winui.ts";
import * as winui from "./winui.ts";

// Svelte's custom renderer for WinUI: a .svelte component's elements are
// WinUI's own controls, named in kebab case (<stack-panel>, <text-block>), so
// the window keeps native controls, Windows' light and dark and the acrylic
// backdrop. The tree itself is svelte-tree.ts; here each element's native
// children follow it (a panel's children, a border's child, a button's
// content, a text block's text).
//
//   class="CardTitle"           the style StyleKey.CardTitle names
//   vertical-alignment="Center" a property; a quoted value becomes the
//                               property's enum, number, flag or Thickness
//   grid.column={1}             an attached property: Grid.setColumn
//   columns="Auto,*,Auto"       a grid's columns (rows= its rows)
//   ontoggled={handler}         an event: onToggled
//   <host element={control} />  a control made elsewhere, placed here

interface WinuiNative {
  control: UIElement | null;
  // What the native collection holds, in order, to change it in place.
  placed: UIElement[];
}

type WinuiElement = ElementNode<WinuiNative>;
type AttributeHandler = (control: UIElement, name: string, value: unknown) => void;
type TextConverter = (text: string, property: string) => unknown;

const hostTag = "host";
const hostAttribute = "element";
const styleAttribute = "class";
const attachedSeparator = ".";
const thicknessProperties = new Set(["margin", "padding", "borderThickness"]);
const exports = new Map<string, unknown>(Object.entries(winui));
const styleKeys = new Map(Object.entries(StyleKey));
const gridLengths = new Map<GridUnit, (length: GridLength) => object>([
  [GridUnit.Auto, () => autoLength()],
  [GridUnit.Pixel, (length: GridLength) => pixelLength(length.value)],
  [GridUnit.Star, (length: GridLength) => starLength(length.value)]
]);

function isRecord(value: unknown): value is Record<string, unknown> {
  return (typeof value === "object" || typeof value === "function") && value !== null;
}

function isControlClass(value: unknown): value is new () => UIElement {
  return typeof value === "function" && "prototype" in value;
}

function isControl(value: unknown): value is UIElement {
  return isRecord(value);
}

function isFrameworkElement(value: unknown): value is FrameworkElement {
  return isRecord(value) && "margin" in value;
}

function read(target: unknown, key: string): unknown {
  return isRecord(target) ? target[key] : undefined;
}

function call(target: unknown, key: string, ...values: unknown[]): unknown {
  const method = read(target, key);
  if (typeof method !== "function") {
    throw new TypeError(`WinUI has no ${key} here`);
  }

  return method.apply(target, values);
}

function write(target: object, key: string, value: unknown): void {
  Object.assign(target, { [key]: value });
}

function nativeLength(text: string): unknown {
  const length = gridLength(text);
  return gridLengths.get(length.unit)?.(length);
}

function nativeThickness(text: string): object {
  return thickness(...thicknessSides(text));
}

function enumMember(property: string, name: string): unknown {
  const typeName = pascalCase(property);
  const member = read(exports.get(typeName), name);
  if (member === undefined) {
    throw new Error(`${property}="${name}" names no ${typeName} (or winui.ts does not export it)`);
  }

  return member;
}

function numberOrMember(text: string, property: string): unknown {
  return isNumeric(text) ? Number(text) : enumMember(property, text);
}

// By the type the property holds now. An object property such as a button's
// content takes a boxed string.
const textConverters = new Map<string, TextConverter>([
  ["boolean", text => text === "true"],
  ["string", text => text],
  ["number", numberOrMember]
]);

function boxedText(text: string): unknown {
  return boxed(text);
}

// A quoted value as the type the property holds now: an enum member by name,
// a number, a flag or a Thickness.
function convert(control: UIElement, property: string, value: unknown): unknown {
  if (typeof value !== "string") {
    return value;
  }

  if (thicknessProperties.has(property)) {
    return nativeThickness(value);
  }

  const converter = textConverters.get(typeof read(control, property)) ?? boxedText;
  return converter(value, property);
}

function applyStyle(control: UIElement, _name: string, value: unknown): void {
  const key = styleKeys.get(String(value));
  if (key === undefined) {
    throw new Error(`class="${String(value)}" names no StyleKey`);
  }

  write(control, "style", style(key));
}

// A grid's columns or rows, each length a new definition; set again, they
// replace the ones before.
function definitionsHandler(type: new () => object, collectionName: string, sizeName: string): AttributeHandler {
  return (control, _name, value) => {
    const collection = read(control, collectionName);
    call(collection, "clear");
    for (const length of splitList(String(value))) {
      const definition = new type();
      write(definition, sizeName, nativeLength(length));
      call(collection, "append", definition);
    }
  };
}

function applyAttached(control: UIElement, name: string, value: unknown): void {
  const [owner = "", attached = ""] = name.split(attachedSeparator);
  const isNumber = typeof value === "string" && isNumeric(value);
  call(exports.get(pascalCase(owner)), `set${pascalCase(attached)}`, control, isNumber ? Number(value) : value);
}

function applyProperty(control: UIElement, name: string, value: unknown): void {
  const property = camelCase(name);
  write(control, property, convert(control, property, value));
}

const namedAttributes = new Map<string, AttributeHandler>([
  [styleAttribute, applyStyle],
  ["columns", definitionsHandler(winui.ColumnDefinition, "columnDefinitions", "width")],
  ["rows", definitionsHandler(winui.RowDefinition, "rowDefinitions", "height")]
]);

function attributeHandler(name: string): AttributeHandler {
  return namedAttributes.get(name) ?? (name.includes(attachedSeparator) ? applyAttached : applyProperty);
}

function applyAttribute(element: WinuiElement, name: string, value: unknown): void {
  const control = element.native.control;
  if (control !== null) {
    attributeHandler(name)(control, name, value);
  }
}

function controlsOf(node: TreeNode<WinuiNative>): UIElement[] {
  const control = node.type === NodeType.Element ? node.native.control : null;
  return control === null ? [] : [control];
}

function textOf(element: WinuiElement): string {
  return element.children.map(child => child.type === NodeType.Text ? child.value : "").join("");
}

// Each control's key in the panels it is placed in.
const controlKeys = new WeakMap<UIElement, string>();
let nextControlKey = 0;

function keyOf(control: UIElement): string {
  const existing = controlKeys.get(control);
  if (existing !== undefined) {
    return existing;
  }

  const key = String(nextControlKey);
  nextControlKey++;
  controlKeys.set(control, key);
  return key;
}

// The native collection as keyed-list sees it, read through `placed`: a
// collection hands back a new wrapper for a control on every read.
function placedItems(element: WinuiElement, collection: unknown): KeyedItems<UIElement> {
  const placed = element.native.placed;
  return {
    get size() {
      return placed.length;
    },
    getAt(index) {
      const control = placed[index];
      if (control === undefined) {
        throw new RangeError(`nothing is placed at ${index}`);
      }

      return control;
    },
    removeAt(index) {
      call(collection, "removeAt", index);
      placed.splice(index, 1);
    },
    insertAt(index, control) {
      call(collection, "insertAt", index, control);
      placed.splice(index, 0, control);
    }
  };
}

// A panel's children, or a ComboBox's items, are changed in place, moving as
// little as they can.
function syncCollection(element: WinuiElement, control: UIElement, collectionName: string): void {
  const wanted = element.children.flatMap(controlsOf);
  const byKey = new Map(wanted.map(child => [keyOf(child), child]));
  syncKeyed({
    items: placedItems(element, read(control, collectionName)),
    keyOf,
    create(key) {
      const child = byKey.get(key);
      if (child === undefined) {
        throw new RangeError(`no control has key ${key}`);
      }

      return child;
    }
  }, [...byKey.keys()]);
}

function syncText(element: WinuiElement, control: UIElement): void {
  write(control, "text", textOf(element));
}

function syncChild(element: WinuiElement, control: UIElement): void {
  const [first = null] = element.children.flatMap(controlsOf);
  write(control, "child", first);
}

// A control, or else the element's text, boxed.
function syncContent(element: WinuiElement, control: UIElement): void {
  const [first] = element.children.flatMap(controlsOf);
  const text = textOf(element).trim();
  const textContent = text === "" ? null : boxed(text);
  write(control, "content", first ?? textContent);
}

// The first of these properties a control has says where its children go.
// Items come before text: a ComboBox has both, and its children are its items.
const childSlots: Array<[string, (element: WinuiElement, control: UIElement) => void]> = [
  ["children", (element, control) => syncCollection(element, control, "children")],
  ["items", (element, control) => syncCollection(element, control, "items")],
  ["text", syncText],
  ["child", syncChild],
  ["content", syncContent]
];

// Brings the native control's children in line with the tree.
function sync(element: WinuiElement): void {
  const control = element.native.control;
  if (control === null) {
    return;
  }

  const slot = childSlots.find(([property]) => property in control);
  slot?.[1](element, control);
}

function syncParent(element: WinuiElement): void {
  const parent = parentElement(element);
  if (parent !== null) {
    sync(parent);
  }
}

// The event a lower-case name (Svelte's ontoggled) stands for: onToggled.
function eventMethod(control: UIElement, type: string): string {
  const wanted = `on${type}`.toLowerCase();
  for (let prototype: unknown = control; isRecord(prototype); prototype = Object.getPrototypeOf(prototype)) {
    const name = Object.getOwnPropertyNames(prototype).find(key => key.toLowerCase() === wanted);
    if (name !== undefined) {
      return name;
    }
  }

  throw new Error(`a ${control.constructor.name} has no ${type} event`);
}

function placeHost(element: WinuiElement, control: unknown): void {
  element.native.control = isControl(control) ? control : null;
  for (const [name, value] of element.attributes) {
    if (name !== hostAttribute) {
      applyAttribute(element, name, value);
    }
  }

  syncParent(element);
}

function createNative(name: string): WinuiNative {
  if (name === hostTag) {
    return {
      control: null,
      placed: []
    };
  }

  const type = exports.get(pascalCase(name));
  if (!isControlClass(type)) {
    throw new Error(`<${name}> is not a WinUI control winui.ts exports`);
  }

  return {
    control: new type(),
    placed: []
  };
}

const winuiHost: NativeHost<WinuiNative> = {
  create: createNative,
  setAttribute(element, name, value) {
    if (name === hostAttribute) {
      placeHost(element, value);
    } else {
      applyAttribute(element, name, value);
    }
  },
  removeAttribute(element, name) {
    if (name === hostAttribute) {
      placeHost(element, null);
    }
  },
  childrenChanged: sync,
  listen(element, type, handler) {
    const control = element.native.control;
    const canListen = control !== null && typeof handler === "function";
    if (!canListen) {
      return null;
    }

    const unsubscribe = call(control, eventMethod(control, type), handler);
    return typeof unsubscribe === "function" ? () => unsubscribe() : null;
  }
};

const renderer = createTreeRenderer(winuiHost);

// The control a mounted component made first: its root.
export function rootControl(fragment: FragmentNode<WinuiNative>): FrameworkElement {
  for (const child of fragment.children) {
    const control = child.type === NodeType.Element ? child.native.control : null;
    if (isFrameworkElement(control)) {
      return control;
    }
  }

  throw new Error("the component made no control");
}

export default renderer;
