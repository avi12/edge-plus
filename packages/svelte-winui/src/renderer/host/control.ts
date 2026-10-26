import type { NativeValue } from "../../native-value.ts";
import { read } from "./reflect.ts";

// The element's native control, handed to code. Svelte runs an attachment in
// an effect of its own, so effects created inside the handler end with the
// attachment: when the element goes, when the expression is evaluated again,
// and when a hot update replaces the component.

type Cleanup = () => void;

// What `{@attach ...}` takes. A custom renderer's element is not a DOM
// element, so it is typed as the plain object it is.
export type NodeAttachment = (node: object) => void | Cleanup;

// A <host> that holds no control (yet) has nothing to hand over.
function nativeControl(node: object): NativeValue {
  return read(read(node, "native"), "control");
}

function isObject(value: NativeValue): value is object {
  return typeof value === "object" && value !== null;
}

// The tag fixes the class, so the control is the type the caller names for
// it; the renderer cannot check that from here.
function isTypedControl<Control extends object>(_value: object): _value is Control {
  return true;
}

// {@attach control<ToggleSwitch>(toggle => { ...; return () => ... })}: the
// handler gets the control, typed by the caller, and may return what undoes
// what it did. This is how a component sets properties and subscribes to
// events: through the control's own typed members, no name mapped from text.
export function control<Control extends object>(handler: (control: Control) => void | Cleanup): NodeAttachment {
  return node => {
    const native = nativeControl(node);
    const isControlMade = isObject(native) && isTypedControl<Control>(native);
    if (isControlMade) {
      return handler(native);
    }
  };
}

// {@attach withControl(StackPanel, panel => ...)}: the same, for code that
// wants the class checked (registering a panel for motion, reading a text
// box); a control of another class is skipped.
export function withControl<Control>(
  type: abstract new (...values: never[]) => Control,
  use: (control: Control) => void
): NodeAttachment {
  return node => {
    const native = nativeControl(node);
    if (native instanceof type) {
      use(native);
    }
  };
}
