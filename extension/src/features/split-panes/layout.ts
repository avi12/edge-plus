// The pane tree: pure geometry, no browser calls, so it runs under `node --test`.
//
// A row split lays its children side by side, a column split stacks them; each child gets its
// weight's share of the split's length. Every pane lives inside one tab; `url` is what it shows,
// empty for the start page.

export enum Axis {
  row = "row",
  column = "column"
}

export type Rect = {
  left: number;
  top: number;
  width: number;
  height: number;
};

export type Pane = { id: string; url: string };
type Split = { axis: Axis; children: Node[]; weights: number[] };
export type Node = Pane | Split;

/** A preset's shape: panes are slot numbers, filled in order by `fromTemplate` */
export type Template = number | { axis: Axis; children: Template[] };

/** The layout in percent of the tab, so CSS places every pane without knowing its size */
export const WHOLE_TAB: Rect = { left: 0, top: 0, width: 100, height: 100 };

/** A divider stops this far from either neighbour's far edge, in percent of their pair */
const MIN_SHARE = 0.08;

export const isPane = (node: Node): node is Pane => "id" in node;

export function panes(root: Node): Pane[] {
  if (isPane(root)) {
    return [root];
  }
  return root.children.flatMap(panes);
}

export const findPane = ({ root, paneId }: { root: Node; paneId: string }) =>
  panes(root).find(pane => pane.id === paneId);

/**
 * Splitting the same way as the pane's own row or column adds a sibling that halves the pane's
 * share; splitting across it nests a new split in the pane's place
 */
export function split({ root, targetId, newPane, axis }: {
  root: Node;
  targetId: string;
  newPane: Pane;
  axis: Axis;
}): Node {
  if (isPane(root)) {
    if (root.id !== targetId) {
      return root;
    }
    return { axis, children: [root, newPane], weights: [1, 1] };
  }
  const iTarget = root.children.findIndex(child => isPane(child) && child.id === targetId);
  const isSiblingSplit = iTarget >= 0 && root.axis === axis;
  if (isSiblingSplit) {
    const half = root.weights[iTarget]! / 2;
    return {
      ...root,
      children: root.children.toSpliced(iTarget + 1, 0, newPane),
      weights: root.weights.toSpliced(iTarget, 1, half, half)
    };
  }
  return {
    ...root,
    children: root.children.map(child => split({ root: child, targetId, newPane, axis }))
  };
}

export function setUrl({ root, paneId, url }: { root: Node; paneId: string; url: string }): Node {
  if (isPane(root)) {
    return root.id === paneId ? { ...root, url } : root;
  }
  return { ...root, children: root.children.map(child => setUrl({ root: child, paneId, url })) };
}

/**
 * A split left with one child is replaced by that child, and a child split along its parent's axis
 * is folded into it, so the tree never grows dead levels
 */
export function remove({ root, paneId }: { root: Node; paneId: string }): Node | null {
  if (isPane(root)) {
    return root.id === paneId ? null : root;
  }
  const children: Node[] = [];
  const weights: number[] = [];
  for (const [i, child] of root.children.entries()) {
    const kept = remove({ root: child, paneId });
    if (!kept) {
      continue;
    }
    const isSameAxisSplit = !isPane(kept) && kept.axis === root.axis;
    if (!isSameAxisSplit) {
      children.push(kept);
      weights.push(root.weights[i]!);
      continue;
    }
    const total = sum(kept.weights);
    children.push(...kept.children);
    weights.push(...kept.weights.map(weight => weight / total * root.weights[i]!));
  }
  if (children.length === 0) {
    return null;
  }
  if (children.length === 1) {
    return children[0]!;
  }
  return { ...root, children, weights };
}

/** Every node's rectangle, keyed by the node object itself */
export function measure({ root, region }: { root: Node; region: Rect }) {
  const rects = new Map<Node, Rect>();

  function place({ node, rect }: { node: Node; rect: Rect }) {
    rects.set(node, rect);
    if (isPane(node)) {
      return;
    }
    const isRow = node.axis === Axis.row;
    const length = isRow ? rect.width : rect.height;
    const total = sum(node.weights);
    let cumulative = 0;
    for (const [i, child] of node.children.entries()) {
      const start = cumulative / total * length;
      cumulative += node.weights[i]!;
      const size = node.weights[i]! / total * length;
      place({
        node: child,
        rect: isRow
          ? { left: rect.left + start, top: rect.top, width: size, height: rect.height }
          : { left: rect.left, top: rect.top + start, width: rect.width, height: size }
      });
    }
  }

  place({ node: root, rect: region });
  return rects;
}

export function layout({ root, region = WHOLE_TAB }: { root: Node; region?: Rect }) {
  const rectByPane = new Map<string, Rect>();
  for (const [node, rect] of measure({ root, region })) {
    if (isPane(node)) {
      rectByPane.set(node.id, rect);
    }
  }
  return rectByPane;
}

export type Divider = {
  /** Path of child indexes from the root to the split the divider belongs to */
  path: number[];
  iBefore: number;
  axis: Axis;
  /** The divider's line, and the split's own box, which a drag is measured against */
  at: Rect;
  within: Rect;
};

/** One divider between every pair of neighbouring children, where a drag resizes the pair */
export function dividers({ root, region = WHOLE_TAB }: { root: Node; region?: Rect }) {
  const rects = measure({ root, region });
  const found: Divider[] = [];

  function walk({ node, path }: { node: Node; path: number[] }) {
    if (isPane(node)) {
      return;
    }
    const within = rects.get(node)!;
    for (const [i, child] of node.children.entries()) {
      walk({ node: child, path: [...path, i] });
      if (i === node.children.length - 1) {
        continue;
      }
      const childRect = rects.get(child)!;
      const isRow = node.axis === Axis.row;
      found.push({
        path,
        iBefore: i,
        axis: node.axis,
        within,
        at: isRow
          ? { left: childRect.left + childRect.width, top: within.top, width: 0, height: within.height }
          : { left: within.left, top: childRect.top + childRect.height, width: within.width, height: 0 }
      });
    }
  }

  walk({ node: root, path: [] });
  return found;
}

/**
 * Moves one divider to `fraction` of the way across its split, taking the space from the two
 * children beside it only, and never closer to either one's far edge than the minimum
 */
export function moveDivider({ root, divider, fraction }: { root: Node; divider: Divider; fraction: number }): Node {
  const tree = structuredClone(root);
  let node: Node = tree;
  for (const i of divider.path) {
    node = (node as Split).children[i]!;
  }
  const split = node as Split;
  const total = sum(split.weights);
  let before = 0;
  for (let i = 0; i < divider.iBefore; i++) {
    before += split.weights[i]!;
  }
  const pairWeight = split.weights[divider.iBefore]! + split.weights[divider.iBefore + 1]!;
  const pairStart = before / total;
  const pairShare = pairWeight / total;
  const wanted = (fraction - pairStart) / pairShare;
  const firstShare = Math.max(MIN_SHARE, Math.min(1 - MIN_SHARE, wanted));
  split.weights[divider.iBefore] = pairWeight * firstShare;
  split.weights[divider.iBefore + 1] = pairWeight * (1 - firstShare);
  return tree;
}

export function fromTemplate({ template, panes: slotPanes }: { template: Template; panes: Pane[] }): Node {
  if (typeof template === "number") {
    return slotPanes[template]!;
  }
  return {
    axis: template.axis,
    children: template.children.map(child => fromTemplate({ template: child, panes: slotPanes })),
    weights: template.children.map(() => 1)
  };
}

export function slotCount(template: Template): number {
  if (typeof template === "number") {
    return 1;
  }
  let count = 0;
  for (const child of template.children) {
    count += slotCount(child);
  }
  return count;
}

function sum(values: number[]) {
  let total = 0;
  for (const value of values) {
    total += value;
  }
  return total;
}
