import { control, type NodeAttachment } from "./control.ts";

// One element's control, held for another element to point at: what an id is
// to aria-labelledby on the web. `{@attach label.attach}` on the label and
// `automationLabeledBy={label.current}` on the control it names, so a screen
// reader reads the label's own text. `current` is reactive, and undefined
// until the element is made and after it goes.
export interface ElementRef<Control extends object> {
  readonly current: Control | undefined;
  readonly attach: NodeAttachment;
}

export function elementRef<Control extends object>(): ElementRef<Control> {
  let current = $state<Control>();
  const attach = control<Control>(made => {
    current = made;
    return () => {
      current = undefined;
    };
  });
  return {
    get current() {
      return current;
    },
    attach
  };
}
