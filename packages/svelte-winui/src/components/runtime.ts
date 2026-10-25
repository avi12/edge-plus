// The one place the renderer's attachment enters the generated components:
// `{@attach control((native: ToggleSwitch) => …)}` hands the handler the
// element's native control and takes back what it returns as the cleanup.
// When the renderer ships its own `control`, this is the line to change.

export type Cleanup = () => void;

function isObject(value: unknown): value is object {
  return typeof value === "object" && value !== null;
}

// The renderer made the control for the tag the component renders, so it is
// the class the handler names.
function isNative<Native>(value: unknown): value is Native {
  return isObject(value);
}

export function control<Native>(use: (native: Native) => Cleanup | void): (element: unknown) => Cleanup | void {
  return element => {
    const holder: unknown = isObject(element) ? Reflect.get(element, "native") : undefined;
    const native: unknown = isObject(holder) ? Reflect.get(holder, "control") : undefined;
    if (isNative<Native>(native)) {
      return use(native);
    }
  };
}
