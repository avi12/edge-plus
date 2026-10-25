import { boxed } from "./layout.ts";
import { style, StyleKey } from "./styles.ts";
import type { FrameworkElement, UIElement } from "./winui.ts";
import * as winui from "./winui.ts";
import { createRenderer } from "svelte/renderer";

// Svelte's custom renderer for WinUI: a .svelte component's elements are
// WinUI's own controls, named in kebab case (<stack-panel>, <text-block>), so
// the window keeps native controls, Windows' light and dark and the acrylic
// backdrop. Svelte keeps its tree of nodes here; each element's native
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

const NodeType = {
  Fragment: "fragment",
  Element: "element",
  Text: "text",
  Comment: "comment"
} as const;

interface ElementNode {
  type: typeof NodeType.Element;
  name: string;
  control: UIElement | null;
  attributes: Map<string, unknown>;
  children: TreeNode[];
  parent: ParentNode | null;
  // What the native collection holds, in order, to change it in place.
  placed: UIElement[];
  unsubscribers: Map<unknown, () => void>;
}

interface FragmentNode {
  type: typeof NodeType.Fragment;
  children: TreeNode[];
  parent: null;
}

interface TextNode {
  type: typeof NodeType.Text;
  value: string;
  parent: ParentNode | null;
}

interface CommentNode {
  type: typeof NodeType.Comment;
  value: string;
  parent: ParentNode | null;
}

type ParentNode = ElementNode | FragmentNode;
type TreeNode = ElementNode | TextNode | CommentNode;
type AnyNode = TreeNode | FragmentNode;

const hostTag = "host";
const hostAttribute = "element";
const styleAttribute = "class";
const definitionAttributes = new Map([
  ["columns", {
    type: winui.ColumnDefinition,
    collection: "columnDefinitions",
    size: "width"
  }],
  ["rows", {
    type: winui.RowDefinition,
    collection: "rowDefinitions",
    size: "height"
  }]
]);
const thicknessProperties = new Set(["margin", "padding", "borderThickness"]);
const autoLength = "Auto";
const starPattern = /^(?<weight>\d*\.?\d*)\*$/u;
const exports = new Map<string, unknown>(Object.entries(winui));
const styleKeys = new Map(Object.entries(StyleKey));

function pascalCase(name: string): string {
  return name.split("-").map(word => `${word.charAt(0).toUpperCase()}${word.slice(1)}`).join("");
}

function camelCase(name: string): string {
  const pascal = pascalCase(name);
  return `${pascal.charAt(0).toLowerCase()}${pascal.slice(1)}`;
}

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

// "8" for every side, "8,4" for left-right and top-bottom, or four sides, as
// in XAML.
function thickness(text: string): object {
  const sides = text.split(",").map(side => Number(side.trim()));
  const [left = 0, top = left, right = left, bottom = top] = sides;
  return {
    left,
    top,
    right,
    bottom
  };
}

// "Auto", "*", "2*" or pixels, as in XAML.
function gridLength(text: string): object {
  const star = starPattern.exec(text);
  if (text === autoLength) {
    return {
      value: 1,
      gridUnitType: winui.GridUnitType.Auto
    };
  }

  if (star !== null) {
    const weight = star.groups?.["weight"] ?? "";
    return {
      value: weight === "" ? 1 : Number(weight),
      gridUnitType: winui.GridUnitType.Star
    };
  }

  return {
    value: Number(text),
    gridUnitType: winui.GridUnitType.Pixel
  };
}

// A quoted value as the type the property holds now: an enum member by name,
// a number, a flag or a Thickness.
function convert(control: UIElement, property: string, value: unknown): unknown {
  if (typeof value !== "string") {
    return value;
  }

  if (thicknessProperties.has(property)) {
    return thickness(value);
  }

  const current = read(control, property);
  if (typeof current === "boolean") {
    return value === "true";
  }

  if (typeof current === "string") {
    return value;
  }

  // An object property such as a button's content takes a boxed string.
  if (typeof current !== "number") {
    return boxed(value);
  }

  const isNumber = value.trim() !== "" && Number.isFinite(Number(value));
  if (isNumber) {
    return Number(value);
  }

  const member = read(exports.get(pascalCase(property)), value);
  if (member === undefined) {
    throw new Error(`${property}="${value}" names no ${pascalCase(property)} (or winui.ts does not export it)`);
  }

  return member;
}

function applyAttribute(element: ElementNode, name: string, value: unknown): void {
  const control = element.control;
  if (control === null) {
    return;
  }

  const definition = definitionAttributes.get(name);
  const [owner, attached] = name.split(".");
  const isAttached = attached !== undefined && owner !== undefined;
  if (name === styleAttribute) {
    const key = styleKeys.get(String(value));
    if (key === undefined) {
      throw new Error(`class="${String(value)}" names no StyleKey`);
    }

    write(control, "style", style(key));
  } else if (definition !== undefined) {
    const collection = read(control, definition.collection);
    for (const length of String(value).split(",")) {
      const row = new definition.type();
      write(row, definition.size, gridLength(length.trim()));
      call(collection, "append", row);
    }
  } else if (isAttached) {
    const isNumber = typeof value === "string" && Number.isFinite(Number(value));
    call(exports.get(pascalCase(owner)), `set${pascalCase(attached)}`, control, isNumber ? Number(value) : value);
  } else {
    const property = camelCase(name);
    write(control, property, convert(control, property, value));
  }
}

function nativeChildren(element: ElementNode): UIElement[] {
  return element.children.flatMap(child => child.type === NodeType.Element && child.control !== null
    ? [child.control]
    : []);
}

function textOf(element: ElementNode): string {
  return element.children.map(child => child.type === NodeType.Text ? child.value : "").join("");
}

// Brings the native control's children in line with the tree, moving as
// little as it can: a panel's collection is changed in place.
function sync(element: ElementNode): void {
  const control = element.control;
  if (control === null) {
    return;
  }

  const wanted = nativeChildren(element);
  const [first = null] = wanted;
  const text = textOf(element).trim();
  if ("children" in control) {
    syncCollection(element, read(control, "children"), wanted);
  } else if ("text" in control) {
    write(control, "text", textOf(element));
  } else if ("child" in control) {
    write(control, "child", first);
  } else if ("content" in control) {
    write(control, "content", first ?? (text === "" ? null : boxed(text)));
  }
}

function syncCollection(element: ElementNode, collection: unknown, wanted: UIElement[]): void {
  const placed = element.placed;
  for (let index = placed.length - 1; index >= 0; index--) {
    const control = placed[index];
    const isWanted = control !== undefined && wanted.includes(control);
    if (!isWanted) {
      call(collection, "removeAt", index);
      placed.splice(index, 1);
    }
  }

  wanted.forEach((control, index) => {
    if (placed[index] === control) {
      return;
    }

    const from = placed.indexOf(control);
    if (from >= 0) {
      call(collection, "removeAt", from);
      placed.splice(from, 1);
    }

    call(collection, "insertAt", index, control);
    placed.splice(index, 0, control);
  });
}

function syncParent(node: TreeNode): void {
  const parent = node.parent;
  if (parent?.type === NodeType.Element) {
    sync(parent);
  }
}

function detach(node: TreeNode): void {
  const parent = node.parent;
  if (parent === null) {
    return;
  }

  parent.children.splice(parent.children.indexOf(node), 1);
  node.parent = null;

  if (parent.type === NodeType.Element) {
    sync(parent);
  }
}

function insert(parent: ParentNode, node: AnyNode, anchor: TreeNode | null): void {
  if (node.type === NodeType.Fragment) {
    // Each insert takes its child out of the fragment.
    for (let child = node.children[0]; child !== undefined; child = node.children[0]) {
      insert(parent, child, anchor);
    }

    return;
  }

  detach(node);
  const position = anchor === null ? -1 : parent.children.indexOf(anchor);
  if (position < 0) {
    parent.children.push(node);
  } else {
    parent.children.splice(position, 0, node);
  }

  node.parent = parent;
  syncParent(node);
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

function placeHost(element: ElementNode, control: unknown): void {
  element.control = isControl(control) ? control : null;
  for (const [name, value] of element.attributes) {
    if (name !== hostAttribute) {
      applyAttribute(element, name, value);
    }
  }

  syncParent(element);
}

const renderer = createRenderer<{
  fragment: FragmentNode;
  element: ElementNode;
  text: TextNode;
  comment: CommentNode;
}>({
  createFragment() {
    return {
      type: NodeType.Fragment,
      children: [],
      parent: null
    };
  },
  createElement(name) {
    const type = exports.get(pascalCase(name));
    const isHost = name === hostTag;
    const isKnownControl = isHost || isControlClass(type);
    if (!isKnownControl) {
      throw new Error(`<${name}> is not a WinUI control winui.ts exports`);
    }

    return {
      type: NodeType.Element,
      name,
      control: isControlClass(type) ? new type() : null,
      attributes: new Map(),
      children: [],
      parent: null,
      placed: [],
      unsubscribers: new Map()
    };
  },
  createTextNode(data) {
    return {
      type: NodeType.Text,
      value: data,
      parent: null
    };
  },
  createComment(data) {
    return {
      type: NodeType.Comment,
      value: data,
      parent: null
    };
  },
  nodeType(node) {
    return node.type;
  },
  getNodeValue(node) {
    return node.value;
  },
  getAttribute(element, name) {
    const value = element.attributes.get(name);
    return value === undefined ? null : String(value);
  },
  setAttribute(element, key, value) {
    element.attributes.set(key, value);

    if (key === hostAttribute) {
      placeHost(element, value);
    } else {
      applyAttribute(element, key, value);
    }
  },
  removeAttribute(element, name) {
    element.attributes.delete(name);

    if (name === hostAttribute) {
      placeHost(element, null);
    }
  },
  hasAttribute(element, name) {
    return element.attributes.has(name);
  },
  setText(node, text) {
    if (node.type === NodeType.Element) {
      node.children = [];
      renderer.insert(node, renderer.createTextNode(text), null);
      return;
    }

    node.value = text;
    syncParent(node);
  },
  getFirstChild(element) {
    return element.children[0] ?? null;
  },
  getLastChild(element) {
    return element.children.at(-1) ?? null;
  },
  getNextSibling(node) {
    const siblings = node.parent?.children ?? [];
    return siblings[siblings.indexOf(node) + 1] ?? null;
  },
  insert(parent, node, anchor) {
    insert(parent, node, anchor);
  },
  remove(node) {
    detach(node);
  },
  getParent(node) {
    return node.parent;
  },
  addEventListener(target, type, handler) {
    const control = target.control;
    const canListen = control !== null && typeof handler === "function";
    if (!canListen) {
      return;
    }

    const unsubscribe = call(control, eventMethod(control, type), handler);
    if (typeof unsubscribe === "function") {
      target.unsubscribers.set(handler, () => unsubscribe());
    }
  },
  removeEventListener(target, _type, handler) {
    target.unsubscribers.get(handler)?.();
    target.unsubscribers.delete(handler);
  }
});

// The control a mounted component made first: its root.
export function rootControl(fragment: FragmentNode): FrameworkElement {
  for (const child of fragment.children) {
    const control = child.type === NodeType.Element ? child.control : null;
    if (isFrameworkElement(control)) {
      return control;
    }
  }

  throw new Error("the component made no control");
}

export default renderer;
