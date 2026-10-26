// The app's own types, shared by its host (launch.ts) and its window: what
// Edge+ generates from Rust, written by hand for this example.
export interface CounterView {
  count: number;
  history: number[];
}

export type CounterAction = { kind: "increment" } | { kind: "fail" };

export const viewName = "view";
