// How many row components the example's list has made and ended: the proof
// that a virtualized list holds a screenful of rows however long it is, and
// that a recycled row does not leak.
export const rowStats = {
  created: 0,
  destroyed: 0
};

export function liveRows(): number {
  return rowStats.created - rowStats.destroyed;
}
