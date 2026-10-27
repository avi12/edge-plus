// The app's own types, shared by its host (launch.ts) and its window: what
// Edge+ generates from Rust, written by hand for this example.
// A type, not an interface: only a type alias is assignable to Json, whose
// objects have an index signature.
export type CounterView = {
  count: number;
  history: number[];
};

export type CounterAction = { kind: "increment" } | { kind: "fail" };

export const viewName = "view";
