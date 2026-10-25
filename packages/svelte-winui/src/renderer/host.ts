import type { StyledElement } from "../css/index.ts";
import { styledElement } from "../css/index.ts";
import type { FrameworkElement, UIElement } from "../winui.ts";
import { ColumnDefinition, Grid, RowDefinition } from "../winui.ts";
import type { KeyedItems } from "./keyed-list.ts";
import { syncKeyed } from "./keyed-list.ts";
import type { GridLength } from "./markup.ts";
import {
  camelCase,
  gridLength,
  GridUnit,
  isNumeric,
  kebabCase,
  pascalCase,
  splitList,
  thicknessSides
} from "./markup.ts";
import { call, isRecord, read, write } from "./reflect.ts";
import type { ElementNode, FragmentNode, NativeHost, TreeNode } from "./tree.ts";
import { createTreeRenderer, NodeType, parentElement } from "./tree.ts";
import {
  autoLength,
  boxed,
  pixelLength,
  starLength,
  thickness
} from "./values.ts";

// Svelte's custom renderer for WinUI: a .svelte component's elements are
// WinUI's own controls, named in kebab case (<stack-panel>, <text-block>), so
// the window keeps native controls, Windows' light and dark and the acrylic
// backdrop. The tree itself is tree.ts; here each element's native children
// follow it (a panel's children, a border's child, a button's content, a text
// block's text).
//
//   class="card-title"          the classes the component's <style> rules match
//                               (css/), after the app's class handler has
//                               seen the string (an app with Styles of its own)
//   style="margin: 4px"         inline declarations, style:name={value} merged
//   vertical-alignment="Center" a property; a quoted value becomes the
//                               property's enum, number, flag or Thickness
//   On-content="On"            a property whose name starts with "on"
//                               (ToggleSwitch.OnContent): Svelte takes a
//                               lower-case "on" for an event, so it is
//                               spelled with a capital, or on-content={() =>
//                               "On"} (see applyPropertyNamedOn)
//   grid.column={1}             an attached property: Grid.setColumn
//   columns="Auto,*,Auto"       a grid's columns (rows= its rows)
//   ontoggled={handler}         an event: onToggled
//   <host element={control} />  a control made elsewhere, placed here
//   {@attach control(...)}      the element's control, handed to code
//                               (control.ts)

interface WinuiNative {
  control: UIElement | null;
  // Made when the element first gets a class or a style.
  styled: StyledElement | null;
  // What the native collection holds, in order, to change it in place.
  placed: UIElement[];
}

type WinuiElement = ElementNode<WinuiNative>;
type AttributeHandler = (control: UIElement, name: string, value: unknown) => void;
type ElementAttributeHandler = (element: WinuiElement, control: UIElement, value: unknown) => void;
type TextConverter = (text: string, property: string) => unknown;

// What an app with Styles of its own makes of `class="..."`, before the
// component's <style> rules see the same string.
export type ClassHandler = (control: UIElement, value: string) => void;

const hostTag = "host";
const hostAttribute = "element";
const classAttribute = "class";
const inlineStyleAttribute = "style";
const attachedSeparator = ".";
const wordSeparator = "-";
const thicknessProperties = new Set(["margin", "padding", "borderThickness"]);
// The classes and enums a tag, an enum-valued attribute or an attached
// property may name, by their WinUI names; the app registers its bindings.
const controls = new Map<string, unknown>();
let classHandler: ClassHandler | null = null;
const gridLengths = new Map<GridUnit, (length: GridLength) => object>([
  [GridUnit.Auto, () => autoLength()],
  [GridUnit.Pixel, (length: GridLength) => pixelLength(length.value)],
  [GridUnit.Star, (length: GridLength) => starLength(length.value)]
]);

// `winui` is the app's one import site of the bindings: every export it has
// becomes a class a tag can name. Later calls add to earlier ones.
export function registerControls(winui: Record<string, unknown>): void {
  for (const [name, value] of Object.entries(winui)) {
    controls.set(name, value);
  }
}

export function setClassHandler(handler: ClassHandler | null): void {
  classHandler = handler;
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

function nativeLength(text: string): unknown {
  const length = gridLength(text);
  return gridLengths.get(length.unit)?.(length);
}

function nativeThickness(text: string): object {
  return thickness(...thicknessSides(text));
}

function enumMember(property: string, name: string): unknown {
  const typeName = pascalCase(property);
  const member = read(controls.get(typeName), name);
  if (member === undefined) {
    throw new Error(`${property}="${name}" names no ${typeName} (or registerControls was not given it)`);
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

// `<host>` has no type of its own: the control placed there names it.
function elementType(element: WinuiElement, control: UIElement): string {
  return element.name === hostTag ? kebabCase(control.constructor.name) : element.name;
}

function stylingOf(element: WinuiElement, control: UIElement): StyledElement {
  element.native.styled ??= styledElement(control, elementType(element, control));
  return element.native.styled;
}

function applyClass(element: WinuiElement, control: UIElement, value: unknown): void {
  const classNames = String(value ?? "");
  classHandler?.(control, classNames);
  const isStyled = element.native.styled !== null || classNames.trim() !== "";
  if (isStyled) {
    stylingOf(element, control).setClasses(classNames);
  }
}

// Svelte hands over the whole style attribute, style:name={value}
// directives merged into it.
function applyInlineStyle(element: WinuiElement, control: UIElement, value: unknown): void {
  const isRemoved = value === null || value === undefined;
  const isUnstyled = isRemoved && element.native.styled === null;
  if (isUnstyled) {
    return;
  }

  stylingOf(element, control).setInlineStyle(isRemoved ? null : String(value));
}

function disposeStyling(element: WinuiElement): void {
  element.native.styled?.dispose();
  element.native.styled = null;
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
  call(controls.get(pascalCase(owner)), `set${pascalCase(attached)}`, control, isNumber ? Number(value) : value);
}

function applyProperty(control: UIElement, name: string, value: unknown): void {
  const property = camelCase(name);
  write(control, property, convert(control, property, value));
}

const elementAttributes = new Map<string, ElementAttributeHandler>([
  [classAttribute, applyClass],
  [inlineStyleAttribute, applyInlineStyle]
]);

const namedAttributes = new Map<string, AttributeHandler>([
  ["columns", definitionsHandler(ColumnDefinition, "columnDefinitions", "width")],
  ["rows", definitionsHandler(RowDefinition, "rowDefinitions", "height")]
]);

function attributeHandler(name: string): AttributeHandler {
  return namedAttributes.get(name) ?? (name.includes(attachedSeparator) ? applyAttached : applyProperty);
}

function applyAttribute(element: WinuiElement, name: string, value: unknown): void {
  const control = element.native.control;
  if (control === null) {
    return;
  }

  const elementAttribute = elementAttributes.get(name);
  if (elementAttribute === undefined) {
    attributeHandler(name)(control, name, value);
  } else {
    elementAttribute(element, control, value);
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

// A property whose name starts with "on" (ToggleSwitch.OnContent) on a raw
// element. Svelte takes every attribute starting with "on" for an event: the
// quoted form is a compile error, and `on-content={value}` hands the host a
// wrapper that calls `value` as a handler and keeps nothing it returns. An
// inline function is handed over as it is, so `on-content={() => "On"}` is
// the form that works, read once when the element is made. A property that
// changes is set through {@attach control(...)}, as components do.
function applyPropertyNamedOn(control: UIElement, type: string, thunk: unknown): void {
  const name = `on${type}`;
  const guidance = `${name} on a raw element must be an inline function returning the value: ${name}={() => value}`;
  if (!isThunk(thunk)) {
    throw new TypeError(guidance);
  }

  const value = valueOf(thunk, guidance);
  if (value === undefined) {
    throw new TypeError(guidance);
  }

  applyProperty(control, name, value);
}

function isThunk(value: unknown): value is () => unknown {
  return typeof value === "function";
}

function valueOf(thunk: () => unknown, guidance: string): unknown {
  try {
    return thunk();
  } catch (error) {
    throw new TypeError(guidance, { cause: error });
  }
}

// Which <host> placed each control, so a hot-swapped component's new root
// can take the old one's place with the same attributes.
const hostOf = new WeakMap<UIElement, WinuiElement>();

function hostControl(element: WinuiElement, control: unknown): void {
  // The styling belongs to the control it was made for.
  disposeStyling(element);
  element.native.control = isControl(control) ? control : null;

  if (element.native.control !== null) {
    hostOf.set(element.native.control, element);
  }
}

function placeHost(element: WinuiElement, control: unknown): void {
  hostControl(element, control);
  for (const [name, value] of element.attributes) {
    if (name !== hostAttribute) {
      applyAttribute(element, name, value);
    }
  }

  syncParent(element);
}

// A removed subtree is discarded, but a control placed in it from elsewhere
// (<host>) lives on and may be placed again, as a hot-swapped component does
// at once: it is taken out of the discarded control that holds it, which
// WinUI would otherwise refuse (already the child of another element).
function releaseHosts(node: TreeNode<WinuiNative>): void {
  if (node.type !== NodeType.Element) {
    return;
  }

  if (node.name === hostTag) {
    disposeStyling(node);
    node.native.control = null;
    syncParent(node);
    return;
  }

  node.children.forEach(releaseHosts);
}

// Nothing in a removed subtree may keep its controls alive: the tree nodes
// hold them, and an event subscription is held by the control itself.
function forgetControls(node: TreeNode<WinuiNative>): void {
  if (node.type !== NodeType.Element) {
    return;
  }

  node.children.forEach(forgetControls);
  for (const subscriptions of node.listeners.values()) {
    subscriptions.forEach(unsubscribe => unsubscribe());
  }

  node.listeners.clear();
  disposeStyling(node);
  node.native.control = null;
  node.native.placed.length = 0;
}

// Hosted controls first: they must leave the discarded controls that hold
// them before those are forgotten.
function release(node: TreeNode<WinuiNative>): void {
  releaseHosts(node);
  forgetControls(node);
}

function createNative(name: string): WinuiNative {
  if (name === hostTag) {
    return {
      control: null,
      styled: null,
      placed: []
    };
  }

  const type = controls.get(pascalCase(name));
  if (!isControlClass(type)) {
    throw new Error(`<${name}> is not a WinUI control registerControls was given`);
  }

  return {
    control: new type(),
    styled: null,
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
    } else if (elementAttributes.has(name)) {
      applyAttribute(element, name, null);
    }
  },
  childrenChanged: sync,
  fragmentChanged(fragment) {
    rootWatchers.get(fragment)?.(firstControl(fragment));
  },
  removed: release,
  listen(element, type, handler) {
    const control = element.native.control;
    return control === null ? null : subscribe(control, type, handler);
  }
};

// What ends the subscription, or null for a handler that is not one.
function subscribe(control: UIElement, type: string, handler: unknown): (() => void) | null {
  const isPropertyNamedOn = type.startsWith(wordSeparator);
  if (isPropertyNamedOn) {
    applyPropertyNamedOn(control, type, handler);
    return null;
  }

  if (typeof handler !== "function") {
    return null;
  }

  const unsubscribe = call(control, eventMethod(control, type), handler);
  return typeof unsubscribe === "function" ? () => unsubscribe() : null;
}

const renderer = createTreeRenderer(winuiHost);

// A mounted component's nodes after Svelte's unmount: whatever is left of
// them lets go of its controls, and the mount target stops being watched.
export function releaseFragment(fragment: FragmentNode<WinuiNative>): void {
  for (const node of fragment.children.splice(0)) {
    node.parent = null;
    release(node);
  }

  unwatchRoot(fragment);
}

function firstControl(parent: FragmentNode<WinuiNative> | WinuiElement): FrameworkElement | null {
  return parent.children.flatMap(controlsOf).find(isFrameworkElement) ?? null;
}

// The control a mounted component made first: its root.
export function rootControl(parent: FragmentNode<WinuiNative> | WinuiElement): FrameworkElement {
  const control = firstControl(parent);
  if (control === null) {
    throw new Error("the component made no control");
  }

  return control;
}

// Told the mount target's first control each time its children change.
const rootWatchers = new WeakMap<FragmentNode<WinuiNative>, (control: FrameworkElement | null) => void>();

export function watchRoot(
  fragment: FragmentNode<WinuiNative>,
  onChange: (control: FrameworkElement | null) => void
): void {
  rootWatchers.set(fragment, onChange);
}

export function unwatchRoot(fragment: FragmentNode<WinuiNative>): void {
  rootWatchers.delete(fragment);
}

const gridPlacements = ["Row", "Column", "RowSpan", "ColumnSpan"];

function replaceInCollection(collection: unknown, previous: FrameworkElement, next: FrameworkElement): void {
  const index = Number(call(collection, "indexOf", previous));
  call(collection, "removeAt", index);
  call(collection, "insertAt", index, next);
}

// Where a control placed by code sits in its parent: a panel's children, a
// border's child or a content control's content.
const parentSlots: Array<[string, (parent: object, previous: FrameworkElement, next: FrameworkElement) => void]> = [
  ["children", (parent, previous, next) => replaceInCollection(read(parent, "children"), previous, next)],
  ["child", (parent, _previous, next) => write(parent, "child", next)],
  ["content", (parent, _previous, next) => write(parent, "content", next)]
];

function replaceInParent(previous: FrameworkElement, next: FrameworkElement): void {
  const parent = read(previous, "parent");
  if (!isRecord(parent)) {
    return;
  }

  const slot = parentSlots.find(([property]) => property in parent);
  if (slot === undefined) {
    return;
  }

  for (const placement of gridPlacements) {
    call(Grid, `set${placement}`, next, call(Grid, `get${placement}`, previous));
  }

  slot[1](parent, previous, next);
}

// Puts `next` where `previous` is: through the <host> that placed it, with
// that host's attributes, or else in its parent control, keeping its place in
// a grid.
export function swapControl(previous: FrameworkElement, next: FrameworkElement): void {
  const host = hostOf.get(previous);
  if (host === undefined) {
    replaceInParent(previous, next);
  } else {
    placeHost(host, next);
  }
}

export default renderer;
