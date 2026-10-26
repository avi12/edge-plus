import type { NativeValue } from "../../native-value.ts";

// Reading and writing a native control by property name, for the renderer,
// which knows a control only by the kebab-case tag it was made from.

export function isRecord(value: NativeValue): value is Record<string, NativeValue> {
  return (typeof value === "object" || typeof value === "function") && value !== null;
}

export function read(target: NativeValue, key: string): NativeValue {
  return isRecord(target) ? target[key] : undefined;
}

export function call(target: NativeValue, key: string, ...values: NativeValue[]): NativeValue {
  const method = read(target, key);
  if (typeof method !== "function") {
    throw new TypeError(`WinUI has no ${key} here`);
  }

  return method.apply(target, values);
}

export function write(target: object, key: string, value: NativeValue): void {
  Object.assign(target, { [key]: value });
}
