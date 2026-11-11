// The geometry of dragging a row of a vertical list to a new place, with no
// WinUI in it: where the row lands, how far each other row slides aside, the
// order that results.

/** Where a dragged row is in a list of rows with these heights, and how far the pointer took it. */
export interface DragState {
  /** The heights of the rows from top to bottom, the dragged one included. */
  heights: readonly number[];
  /** The index of the dragged row. */
  index: number;
  /** How far the pointer moved the row from its place, downward positive. */
  offset: number;
}

function sumOf(heights: readonly number[]) {
  let total = 0;
  for (const height of heights) {
    total += height;
  }

  return total;
}

/** The farthest the row may be moved up (negative) and down: to the first and the last place. */
export function offsetBounds({ heights, index }: Pick<DragState, "heights" | "index">) {
  return {
    min: -sumOf(heights.slice(0, index)),
    max: sumOf(heights.slice(index + 1))
  };
}

/** The offset kept within the list. */
export function clampedOffset(state: DragState) {
  const { min, max } = offsetBounds(state);
  return Math.min(max, Math.max(min, state.offset));
}

/**
 * The index the row lands on: going down it passes a neighbour when its bottom
 * edge crosses the neighbour's middle, going up when its top edge does.
 */
export function targetIndex(state: DragState) {
  const { heights, index } = state;
  const offset = clampedOffset(state);
  const top = sumOf(heights.slice(0, index));
  const height = heights[index] ?? 0;
  const isDownward = offset > 0;
  let target = index;
  if (isDownward) {
    const bottomEdge = top + height + offset;
    let neighbourTop = top + height;
    for (const neighbourHeight of heights.slice(index + 1)) {
      const isPassed = neighbourTop + neighbourHeight / 2 < bottomEdge;
      if (!isPassed) {
        break;
      }

      target++;
      neighbourTop += neighbourHeight;
    }

    return target;
  }

  const topEdge = top + offset;
  let neighbourBottom = top;
  for (const neighbourHeight of heights.slice(0, index).reverse()) {
    const isPassed = neighbourBottom - neighbourHeight / 2 > topEdge;
    if (!isPassed) {
      break;
    }

    target--;
    neighbourBottom -= neighbourHeight;
  }

  return target;
}

/** How far each row sits from its own place while the dragged one is over `target`; the dragged row itself is 0. */
export function slideOffsets({ heights, index, target }: Pick<DragState, "heights" | "index"> & { target: number }) {
  const draggedHeight = heights[index] ?? 0;
  return heights.map((_height, iRow) => {
    const isDownward = target > index;
    const isPassed = isDownward ? iRow > index && iRow <= target : iRow >= target && iRow < index;
    if (!isPassed) {
      return 0;
    }

    return isDownward ? -draggedHeight : draggedHeight;
  });
}

/** How far the dragged row sits from its own place once it is in the slot of `target`. */
export function slotOffset({ heights, index, target }: Pick<DragState, "heights" | "index"> & { target: number }) {
  const isDownward = target > index;
  if (isDownward) {
    return sumOf(heights.slice(index + 1, target + 1));
  }

  return 0 - sumOf(heights.slice(target, index));
}

/** The ids with the one at `index` moved to `target`. */
export function movedIds({ ids, index, target }: {
  ids: readonly string[];
  index: number;
  target: number;
}) {
  const moved = [...ids];
  const [dragged] = moved.splice(index, 1);
  if (dragged !== undefined) {
    moved.splice(target, 0, dragged);
  }

  return moved;
}
