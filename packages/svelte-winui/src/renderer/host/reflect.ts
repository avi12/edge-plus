// Reading and writing a native control by property name, for the renderer,
// which knows a control only by the kebab-case tag it was made from.

export function isRecord(value: unknown): value is Record<string, unknown> {
  return (typeof value === "object" || typeof value === "function") && value !== null;
}

export function read(target: unknown, key: string): unknown {
  return isRecord(target) ? target[key] : undefined;
}

export function call(target: unknown, key: string, ...values: unknown[]): unknown {
  const method = read(target, key);
  if (typeof method !== "function") {
    throw new TypeError(`WinUI has no ${key} here`);
  }

  return method.apply(target, values);
}

export function write(target: object, key: string, value: unknown): void {
  Object.assign(target, { [key]: value });
}
