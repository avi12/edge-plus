import { createRenderer } from "svelte/renderer";

// The tree of nodes a Svelte custom renderer keeps, with no platform in it:
// elements, text, the comments Svelte anchors its {#if} and {#each} blocks
// on, and the fragments it builds them in. Each element carries the native
// object it stands for; the host is told when an element's children change,
// so it can bring the native children in line.

export const NodeType = {
  Fragment: "fragment",
  Element: "element",
  Text: "text",
  Comment: "comment"
} as const;

export interface ElementNode<Native> {
  type: typeof NodeType.Element;
  name: string;
  native: Native;
  attributes: Map<string, unknown>;
  children: Array<TreeNode<Native>>;
  parent: ParentNode<Native> | null;
  // By event type, then by handler: what ends each subscription.
  listeners: Map<string, Map<unknown, () => void>>;
}

export interface FragmentNode<Native> {
  type: typeof NodeType.Fragment;
  children: Array<TreeNode<Native>>;
  parent: null;
}

export interface TextNode<Native> {
  type: typeof NodeType.Text;
  value: string;
  parent: ParentNode<Native> | null;
}

export interface CommentNode<Native> {
  type: typeof NodeType.Comment;
  value: string;
  parent: ParentNode<Native> | null;
}

export type ParentNode<Native> = ElementNode<Native> | FragmentNode<Native>;
export type TreeNode<Native> = ElementNode<Native> | TextNode<Native> | CommentNode<Native>;
type AnyNode<Native> = TreeNode<Native> | FragmentNode<Native>;

// What the tree asks of the platform its elements stand for.
export interface NativeHost<Native> {
  create(name: string): Native;
  setAttribute(element: ElementNode<Native>, name: string, value: unknown): void;
  removeAttribute(element: ElementNode<Native>, name: string): void;
  childrenChanged(element: ElementNode<Native>): void;
  // A fragment's children changed: a mount target's, when a hot update
  // replaces a component's nodes.
  fragmentChanged(fragment: FragmentNode<Native>): void;
  // A subtree Svelte has removed and will not insert again.
  removed(node: TreeNode<Native>): void;
  // What ends the subscription, or null when nothing was subscribed.
  listen(element: ElementNode<Native>, type: string, handler: unknown): (() => void) | null;
}

export function parentElement<Native>(node: TreeNode<Native>): ElementNode<Native> | null {
  return node.parent?.type === NodeType.Element ? node.parent : null;
}

function notify<Native>(host: NativeHost<Native>, parent: ParentNode<Native> | null): void {
  if (parent?.type === NodeType.Element) {
    host.childrenChanged(parent);
  } else if (parent?.type === NodeType.Fragment) {
    host.fragmentChanged(parent);
  }
}

// Takes a node out of its parent's children, leaving the host untold.
function unlink<Native>(node: TreeNode<Native>): ParentNode<Native> | null {
  const parent = node.parent;
  if (parent === null) {
    return null;
  }

  parent.children.splice(parent.children.indexOf(node), 1);
  node.parent = null;
  return parent;
}

// Puts an unlinked node before the anchor, or last when there is none.
function link<Native>(parent: ParentNode<Native>, node: TreeNode<Native>, anchor: TreeNode<Native> | null): void {
  const position = anchor === null ? -1 : parent.children.indexOf(anchor);
  if (position < 0) {
    parent.children.push(node);
  } else {
    parent.children.splice(position, 0, node);
  }

  node.parent = parent;
}

function removeNode<Native>(host: NativeHost<Native>, node: TreeNode<Native>): void {
  notify(host, unlink(node));
}

// Inserting a fragment moves all its children; the parent hears of it once.
function insertFragment<Native>(
  host: NativeHost<Native>,
  parent: ParentNode<Native>,
  fragment: FragmentNode<Native>,
  anchor: TreeNode<Native> | null
): void {
  for (const child of fragment.children.splice(0)) {
    child.parent = null;
    link(parent, child, anchor);
  }

  notify(host, parent);
}

// A node already in the tree is moved: a keyed {#each} reorders this way. A
// move within one parent tells the host once, after the move.
function insertNode<Native>(
  host: NativeHost<Native>,
  parent: ParentNode<Native>,
  node: AnyNode<Native>,
  anchor: TreeNode<Native> | null
): void {
  if (node.type === NodeType.Fragment) {
    insertFragment(host, parent, node, anchor);
    return;
  }

  if (node === anchor) {
    return;
  }

  const previousParent = unlink(node);
  link(parent, node, anchor);

  if (previousParent !== parent) {
    notify(host, previousParent);
  }

  notify(host, parent);
}

export function nextSibling<Native>(node: TreeNode<Native>): TreeNode<Native> | null {
  const siblings = node.parent?.children ?? [];
  return siblings[siblings.indexOf(node) + 1] ?? null;
}

// An element's text replaces all its children with one text node.
function setElementText<Native>(host: NativeHost<Native>, element: ElementNode<Native>, text: string): void {
  for (const child of element.children.splice(0)) {
    child.parent = null;
  }

  link(element, createText<Native>(text), null);
  notify(host, element);
}

function createText<Native>(value: string): TextNode<Native> {
  return {
    type: NodeType.Text,
    value,
    parent: null
  };
}

function listenersOf<Native>(element: ElementNode<Native>, type: string): Map<unknown, () => void> {
  const existing = element.listeners.get(type);
  if (existing !== undefined) {
    return existing;
  }

  const created = new Map<unknown, () => void>();
  element.listeners.set(type, created);
  return created;
}

export function createTreeRenderer<Native>(host: NativeHost<Native>) {
  return createRenderer<{
    fragment: FragmentNode<Native>;
    element: ElementNode<Native>;
    text: TextNode<Native>;
    comment: CommentNode<Native>;
  }>({
    createFragment() {
      return {
        type: NodeType.Fragment,
        children: [],
        parent: null
      };
    },
    createElement(name) {
      return {
        type: NodeType.Element,
        name,
        native: host.create(name),
        attributes: new Map(),
        children: [],
        parent: null,
        listeners: new Map()
      };
    },
    createTextNode(data) {
      return createText(data);
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
      host.setAttribute(element, key, value);
    },
    removeAttribute(element, name) {
      element.attributes.delete(name);
      host.removeAttribute(element, name);
    },
    hasAttribute(element, name) {
      return element.attributes.has(name);
    },
    setText(node, text) {
      if (node.type === NodeType.Element) {
        setElementText(host, node, text);
        return;
      }

      node.value = text;
      notify(host, node.parent);
    },
    getFirstChild(element) {
      return element.children[0] ?? null;
    },
    getLastChild(element) {
      return element.children.at(-1) ?? null;
    },
    getNextSibling(node) {
      return nextSibling(node);
    },
    insert(parent, node, anchor) {
      insertNode(host, parent, node, anchor);
    },
    remove(node) {
      removeNode(host, node);
      host.removed(node);
    },
    getParent(node) {
      return node.parent;
    },
    addEventListener(target, type, handler) {
      const unsubscribe = host.listen(target, type, handler);
      if (unsubscribe !== null) {
        listenersOf(target, type).set(handler, unsubscribe);
      }
    },
    removeEventListener(target, type, handler) {
      const listeners = target.listeners.get(type);
      listeners?.get(handler)?.();
      listeners?.delete(handler);
    }
  });
}
