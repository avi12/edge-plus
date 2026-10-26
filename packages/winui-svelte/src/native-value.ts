import type { PropertyValue } from "./winui.ts";

// What a WinRT property holds, a markup attribute carries or a native call
// takes and returns: a plain value, or the object (WinRT projection, function,
// array) it already is. The one name for "any of these" across the CSS runtime,
// the renderer and the components.
export type NativeValue = string | number | boolean | bigint | object | null | undefined;

// What the bindings return for a WinRT object they cannot name (IInspectable:
// a boxed string, a looked-up resource). They type it by its width, not by
// what it is, so this alias follows them and `boxedObject` narrows it.
export type BoxedValue = ReturnType<typeof PropertyValue.createString>;

export function boxedObject(box: BoxedValue): object {
  const isObject = typeof box === "object" && box !== null;
  if (!isObject) {
    throw new TypeError("the bindings returned a boxed value that is not an object");
  }

  return box;
}
