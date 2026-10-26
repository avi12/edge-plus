import { write } from "../renderer/reflect.ts";
import type { SystemBackdrop, Window } from "../winui.ts";
import { DesktopAcrylicBackdrop, MicaBackdrop, MicaKind } from "../winui.ts";

// The material behind a window's content. Acrylic blurs and tints what is
// behind the window; Mica tints with the desktop wallpaper (Mica Alt more
// strongly, as a tabbed title bar has it); none leaves the theme's solid color.
export const Backdrop = {
  Acrylic: "acrylic",
  Mica: "mica",
  MicaAlt: "micaAlt",
  None: "none"
} as const;

export type Backdrop = (typeof Backdrop)[keyof typeof Backdrop];

// A new backdrop each time: WinUI ties one to the window that holds it.
function systemBackdropFor(backdrop: Backdrop): SystemBackdrop | null {
  if (backdrop === Backdrop.Acrylic) {
    return new DesktopAcrylicBackdrop();
  }

  if (backdrop === Backdrop.Mica) {
    return new MicaBackdrop();
  }

  if (backdrop === Backdrop.MicaAlt) {
    const micaAlt = new MicaBackdrop();
    micaAlt.kind = MicaKind.BaseAlt;
    return micaAlt;
  }

  return null;
}

export function applyBackdrop(window: Window, backdrop: Backdrop): void {
  const systemBackdrop = systemBackdropFor(backdrop);
  if (systemBackdrop === null) {
    // The binding types the property as non-null, but null is how WinUI takes
    // a backdrop away.
    write(window, "systemBackdrop", null);
    return;
  }

  window.systemBackdrop = systemBackdrop;
}
