// The pane tree: pure geometry, no browser calls, so it runs under `node --test`.
//
// A row split lays its children side by side, a column split stacks them; each child gets its
// weight's share of the split's length, minus the gaps.

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

type Pane = { windowId: number };
type Split = { axis: Axis; children: Node[]; weights: number[] };
export type Node = Pane | Split;

/** A preset's shape: panes are slot numbers, filled in order by `fromTemplate` */
export type Template = number | { axis: Axis; children: Template[] };

/** The smallest a pane may be; Edge refuses to make a window any smaller */
export type Size = { width: number; height: number };
const BOUNDS_TOLERANCE = 2;

export const pane = (windowId: number): Pane => ({ windowId });

export const isPane = (node: Node): node is Pane => "windowId" in node;

export function panes(root: Node): number[] {
  if (isPane(root)) {
    return [root.windowId];
  }
  return root.children.flatMap(panes);
}

export const contains = ({ root, windowId }: { root: Node; windowId: number }) =>
  panes(root).includes(windowId);

/**
 * Splitting the same way as the pane's own row or column adds a sibling that halves the pane's
 * share; splitting across it nests a new split in the pane's place
 */
export function split({ root, targetWindowId, newWindowId, axis }: {
  root: Node;
  targetWindowId: number;
  newWindowId: number;
  axis: Axis;
}): Node {
  if (isPane(root)) {
    if (root.windowId !== targetWindowId) {
      return root;
    }
    return { axis, children: [root, pane(newWindowId)], weights: [1, 1] };
  }
  const iTarget = root.children.findIndex(child => isPane(child) && child.windowId === targetWindowId);
  const isSiblingSplit = iTarget >= 0 && root.axis === axis;
  if (isSiblingSplit) {
    const half = root.weights[iTarget]! / 2;
    return {
      ...root,
      children: root.children.toSpliced(iTarget + 1, 0, pane(newWindowId)),
      weights: root.weights.toSpliced(iTarget, 1, half, half)
    };
  }
  return {
    ...root,
    children: root.children.map(child => split({ root: child, targetWindowId, newWindowId, axis }))
  };
}

export function replace({ root, fromWindowId, toWindowId }: {
  root: Node;
  fromWindowId: number;
  toWindowId: number;
}): Node {
  if (isPane(root)) {
    return root.windowId === fromWindowId ? pane(toWindowId) : root;
  }
  return {
    ...root,
    children: root.children.map(child => replace({ root: child, fromWindowId, toWindowId }))
  };
}

/**
 * A split left with one child is replaced by that child, and a child split along its parent's axis
 * is folded into it, so the tree never grows dead levels
 */
export function remove({ root, windowId }: { root: Node; windowId: number }): Node | null {
  if (isPane(root)) {
    return root.windowId === windowId ? null : root;
  }
  const children: Node[] = [];
  const weights: number[] = [];
  for (const [i, child] of root.children.entries()) {
    const kept = remove({ root: child, windowId });
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
export function measure({ root, region, gap }: { root: Node; region: Rect; gap: number }) {
  const rects = new Map<Node, Rect>();

  function place({ node, rect }: { node: Node; rect: Rect }) {
    rects.set(node, rect);
    if (isPane(node)) {
      return;
    }
    const isRow = node.axis === Axis.row;
    const length = (isRow ? rect.width : rect.height) - gap * (node.children.length - 1);
    const total = sum(node.weights);
    let cumulative = 0;
    for (const [i, child] of node.children.entries()) {
      // From the running total rather than per child, so the panes always add up to the region
      const start = Math.round(cumulative / total * length);
      cumulative += node.weights[i]!;
      const end = Math.round(cumulative / total * length);
      const offset = start + i * gap;
      const size = end - start;
      place({
        node: child,
        rect: isRow
          ? { left: rect.left + offset, top: rect.top, width: size, height: rect.height }
          : { left: rect.left, top: rect.top + offset, width: rect.width, height: size }
      });
    }
  }

  place({ node: root, rect: region });
  return rects;
}

export function layout({ root, region, gap }: { root: Node; region: Rect; gap: number }) {
  const rectByWindow = new Map<number, Rect>();
  for (const [node, rect] of measure({ root, region, gap })) {
    if (isPane(node)) {
      rectByWindow.set(node.windowId, rect);
    }
  }
  return rectByWindow;
}

/** Whether every pane gets at least the minimum, so a split or preset can be offered at all */
export function fits({ root, region, gap, minimum }: { root: Node; region: Rect; gap: number; minimum: Size }) {
  for (const rect of layout({ root, region, gap }).values()) {
    const isBigEnough = rect.width >= minimum.width && rect.height >= minimum.height;
    if (!isBigEnough) {
      return false;
    }
  }
  return true;
}

export function fromTemplate({ template, windowIds }: { template: Template; windowIds: number[] }): Node {
  if (typeof template === "number") {
    return pane(windowIds[template]!);
  }
  return {
    axis: template.axis,
    children: template.children.map(child => fromTemplate({ template: child, windowIds })),
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

/**
 * Windows carry an invisible resize border on their left, right and bottom, inside the bounds Edge
 * reports, so panes abutting by their bounds show a seam twice that wide. Growing each pane by the
 * border on every side it shares with a neighbour makes the visible frames touch; the outer edges
 * stay, since the region was measured off a real window's bounds, border included
 */
export function toWindowBounds({ rect, region, border }: { rect: Rect; region: Rect; border: number }) {
  const isInnerLeft = rect.left > region.left;
  const isInnerRight = rect.left + rect.width < region.left + region.width;
  const isInnerBottom = rect.top + rect.height < region.top + region.height;
  const left = rect.left - (isInnerLeft ? border : 0);
  return {
    left,
    top: rect.top,
    width: rect.left + rect.width + (isInnerRight ? border : 0) - left,
    height: rect.height + (isInnerBottom ? border : 0)
  };
}

/** The inverse, for bounds the user dragged a pane to; inner sides are read off its place before */
export function fromWindowBounds({ bounds, rect, region, border }: {
  bounds: Rect;
  rect: Rect;
  region: Rect;
  border: number;
}) {
  const grown = toWindowBounds({ rect, region, border });
  const leftGrowth = rect.left - grown.left;
  const rightGrowth = grown.left + grown.width - (rect.left + rect.width);
  const left = bounds.left + leftGrowth;
  const right = bounds.left + bounds.width - rightGrowth;
  return {
    left,
    top: bounds.top,
    width: right - left,
    height: bounds.height - (grown.height - rect.height)
  };
}

type PathStep = { node: Node; iChild: number };
type Edge = { axis: Axis; isStart: boolean; at: number; was: number };

const differs = (a: number, b: number) => Math.abs(a - b) > BOUNDS_TOLERANCE;

/**
 * The user dragged one pane's edge, or the whole pane. An inner edge moves the boundary it sits on,
 * an outer edge grows or shrinks the whole group, and a move without a resize carries the group along
 */
export function resize({ root, region, gap, windowId, wanted, minimum }: {
  root: Node;
  region: Rect;
  gap: number;
  windowId: number;
  wanted: Rect;
  minimum: Size;
}) {
  const tree = structuredClone(root);
  const rects = measure({ root: tree, region, gap });
  const path = pathTo({ node: tree, windowId });
  if (!path) {
    return { root, region };
  }
  const current = rects.get(path.at(-1)!.node)!;
  const isSameSize = !differs(current.width, wanted.width) && !differs(current.height, wanted.height);
  if (isSameSize) {
    return {
      root,
      region: {
        ...region,
        left: region.left + wanted.left - current.left,
        top: region.top + wanted.top - current.top
      }
    };
  }

  const nextRegion = { ...region };
  const edges: Edge[] = [
    { axis: Axis.row, isStart: true, at: wanted.left, was: current.left },
    { axis: Axis.row, isStart: false, at: wanted.left + wanted.width, was: current.left + current.width },
    { axis: Axis.column, isStart: true, at: wanted.top, was: current.top },
    { axis: Axis.column, isStart: false, at: wanted.top + wanted.height, was: current.top + current.height }
  ];
  for (const edge of edges) {
    if (!differs(edge.at, edge.was)) {
      continue;
    }
    // The deepest split with a boundary on this side of the pane owns the edge; with none, it is
    // the group's own
    const owner = path.slice(0, -1).findLast(({ node, iChild }) => {
      const isSplitAlongEdge = !isPane(node) && node.axis === edge.axis;
      const hasNeighbourOnThatSide = edge.isStart ? iChild > 0 : !isPane(node) && iChild < node.children.length - 1;
      return isSplitAlongEdge && hasNeighbourOnThatSide;
    });
    if (!owner) {
      moveRegionEdge({ region: nextRegion, edge });
      continue;
    }
    moveBoundary({
      split: owner.node as Split,
      iBefore: edge.isStart ? owner.iChild - 1 : owner.iChild,
      edge,
      rects,
      gap,
      minimum
    });
  }
  return { root: tree, region: nextRegion };
}

function moveBoundary({ split, iBefore, edge, rects, gap, minimum }: {
  split: Split;
  iBefore: number;
  edge: Edge;
  rects: Map<Node, Rect>;
  gap: number;
  minimum: Size;
}) {
  const isRow = split.axis === Axis.row;
  const minimumSize = isRow ? minimum.width : minimum.height;
  const first = rects.get(split.children[iBefore]!)!;
  const second = rects.get(split.children[iBefore + 1]!)!;
  const start = isRow ? first.left : first.top;
  const end = isRow ? second.left + second.width : second.top + second.height;
  const room = end - start - gap;
  const hasRoomForBoth = room >= 2 * minimumSize;
  if (!hasRoomForBoth) {
    return;
  }
  // A start edge is where the second child begins; an end edge is where the first one stops
  const wantedFirstSize = edge.isStart ? edge.at - gap - start : edge.at - start;
  const firstSize = Math.max(minimumSize, Math.min(room - minimumSize, wantedFirstSize));
  const pairWeight = split.weights[iBefore]! + split.weights[iBefore + 1]!;
  split.weights[iBefore] = pairWeight * firstSize / room;
  split.weights[iBefore + 1] = pairWeight - split.weights[iBefore];
}

function moveRegionEdge({ region, edge }: { region: Rect; edge: Edge }) {
  const isRow = edge.axis === Axis.row;
  const startKey = isRow ? "left" : "top";
  const sizeKey = isRow ? "width" : "height";
  if (!edge.isStart) {
    region[sizeKey] = edge.at - region[startKey];
    return;
  }
  const end = region[startKey] + region[sizeKey];
  region[startKey] = edge.at;
  region[sizeKey] = end - edge.at;
}

/** Root first, the pane last; each split step carries the child the path goes through */
function pathTo({ node, windowId }: { node: Node; windowId: number }): PathStep[] | null {
  if (isPane(node)) {
    return node.windowId === windowId ? [{ node, iChild: -1 }] : null;
  }
  for (const [iChild, child] of node.children.entries()) {
    const rest = pathTo({ node: child, windowId });
    if (rest) {
      return [{ node, iChild }, ...rest];
    }
  }
  return null;
}

function sum(values: number[]) {
  let total = 0;
  for (const value of values) {
    total += value;
  }
  return total;
}
