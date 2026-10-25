import type { ElementNode, FragmentNode, NativeHost, TreeNode } from "./svelte-tree.ts";
import { createTreeRenderer, NodeType } from "./svelte-tree.ts";

// A host with no platform behind it, for the tests: each element counts how
// often its children changed and how many events it listens to; its
// attributes are kept by the tree.

interface ProbeNative {
  childChanges: number;
  subscriptions: number;
}

export type ProbeElement = ElementNode<ProbeNative>;

const probeHost: NativeHost<ProbeNative> = {
  create() {
    return {
      childChanges: 0,
      subscriptions: 0
    };
  },
  setAttribute() {},
  removeAttribute() {},
  childrenChanged(element) {
    element.native.childChanges++;
  },
  listen(element) {
    element.native.subscriptions++;
    return () => {
      element.native.subscriptions--;
    };
  }
};

const probeRenderer = createTreeRenderer(probeHost);

// The tree as text: an element as name(children), text in quotes, a comment
// as #. Fragments show their children.
export function outline(node: TreeNode<ProbeNative> | FragmentNode<ProbeNative>): string {
  if (node.type === NodeType.Text) {
    return `"${node.value}"`;
  }

  if (node.type === NodeType.Comment) {
    return "#";
  }

  const children = node.children.map(outline).join(" ");
  return node.type === NodeType.Element ? `${node.name}(${children})` : children;
}

export default probeRenderer;
