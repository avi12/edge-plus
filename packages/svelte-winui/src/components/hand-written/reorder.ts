// Moving one key within an order, for the keyboard reorder (Alt+Up/Down).

export const MoveDirection = {
  Up: -1,
  Down: 1
} as const;

export type MoveDirection = (typeof MoveDirection)[keyof typeof MoveDirection];

// Where `key` lands when it moves one place; null when it is not in the
// order or already at that end.
export function movedIndex(order: readonly string[], key: string, direction: MoveDirection): number | null {
  const index = order.indexOf(key);
  const target = index + direction;
  const isMovable = index >= 0 && target >= 0 && target < order.length;
  return isMovable ? target : null;
}

export function sameOrder(first: readonly string[], second: readonly string[]): boolean {
  return first.length === second.length && first.every((key, index) => key === second[index]);
}
