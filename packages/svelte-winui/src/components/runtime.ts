// What every generated component imports: the renderer's attachment, which
// hands the component the element's native control, and, as a side effect of
// importing it, the registration of the generated bindings with the renderer,
// so a `<toggle-switch>` a component renders is made as a ToggleSwitch.

import { registerControls } from "../renderer/host.ts";
import * as bindings from "./generated/bindings.ts";

export { control } from "../renderer/control.ts";

registerControls(bindings);
