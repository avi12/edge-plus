import type { Rect } from "@/features/split-panes/layout";

export enum Direction {
  left = "left",
  right = "right",
  up = "up",
  down = "down"
}

/** Panes touch within this many pixels once their invisible borders overlap */
const EDGE_TOLERANCE = 2;

const right = (rect: Rect) => rect.left + rect.width;
const bottom = (rect: Rect) => rect.top + rect.height;

/** How far `candidate` lies past `from` in `direction`; negative when it does not lie that way */
function gap({ from, candidate, direction }: { from: Rect; candidate: Rect; direction: Direction }) {
  switch (direction) {
    case Direction.left:
      return from.left - right(candidate);
    case Direction.right:
      return candidate.left - right(from);
    case Direction.up:
      return from.top - bottom(candidate);
    case Direction.down:
      return candidate.top - bottom(from);
  }
}

/** How much of the two panes' shared side faces each other */
function overlap({ from, candidate, direction }: { from: Rect; candidate: Rect; direction: Direction }) {
  const isHorizontal = direction === Direction.left || direction === Direction.right;
  if (isHorizontal) {
    return Math.min(bottom(from), bottom(candidate)) - Math.max(from.top, candidate.top);
  }
  return Math.min(right(from), right(candidate)) - Math.max(from.left, candidate.left);
}

/**
 * The pane on that side of `fromWindowId`, the way a tiling window manager picks it: the nearest
 * one past that edge that faces it, the one facing it most when several are as near
 */
export function neighbour({ rects, fromWindowId, direction }: {
  rects: Map<number, Rect>;
  fromWindowId: number;
  direction: Direction;
}) {
  const from = rects.get(fromWindowId);
  if (!from) {
    return null;
  }
  let best: { windowId: number; distance: number; facing: number } | null = null;
  for (const [windowId, candidate] of rects) {
    const distance = gap({ from, candidate, direction });
    const facing = overlap({ from, candidate, direction });
    const isOnThatSide = windowId !== fromWindowId && distance >= -EDGE_TOLERANCE && facing > 0;
    if (!isOnThatSide) {
      continue;
    }
    const isBetter = !best || distance < best.distance - EDGE_TOLERANCE ||
      (Math.abs(distance - best.distance) <= EDGE_TOLERANCE && facing > best.facing);
    if (isBetter) {
      best = { windowId, distance, facing };
    }
  }
  return best?.windowId ?? null;
}
