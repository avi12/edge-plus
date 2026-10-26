import type { Action } from "../../protocol.g.ts";
import type { LocalThemeKind, RunAction } from "../cards/theming-card.ts";
import { pickLocalTheme } from "../theming/local-theme-picker.ts";
import { getContext, setContext } from "svelte";
import type { WindowHandle } from "svelte-winui/components";

const statusMilliseconds = 4000;

// What the page asks of the process it runs in.
export interface PanelActions {
  run: RunAction;
  restartToUpdate(): Promise<string | null>;
  // Calls back after the delay; the result cancels it.
  after(milliseconds: number, callback: () => void): () => void;
}

// The page's own state and the actions its components run, shared down the
// tree through context (providePanel / usePanel). The View is not here: the
// page reads it from the host's state.
export class Panel {
  // The error of an action that failed, shown under the update card until the
  // user closes it. The core's own notices land here too.
  actionNotice = $state("");
  // While files are dragged over the window.
  isVeilShown = $state(false);
  // What the core says in the title bar, shown for a moment.
  statusLine = $state("");
  isStatusShown = $state(false);
  // The open window (shell.svelte binds it), which owns the pickers.
  window = $state.raw<WindowHandle | undefined>();
  private readonly actions: PanelActions;
  private cancelStatus: (() => void) | null = null;

  constructor(actions: PanelActions) {
    this.actions = actions;
  }

  run(action: Action): Promise<string | null> {
    return this.actions.run(action);
  }

  // The components call these through usePanel(), which fallow does not trace
  // from a .svelte file.
  // fallow-ignore-next-line unused-class-member
  pick(kind: LocalThemeKind): Promise<string | null> {
    if (this.window === undefined) {
      throw new Error("the window is not open");
    }

    return pickLocalTheme(kind, this.window);
  }

  // fallow-ignore-next-line unused-class-member
  restartToUpdate(): Promise<string | null> {
    return this.actions.restartToUpdate();
  }

  // fallow-ignore-next-line unused-class-member
  async runAndReport(action: Action): Promise<void> {
    this.report(await this.run(action));
  }

  report(error: string | null): void {
    if (error !== null) {
      this.actionNotice = error;
    }
  }

  showStatus(line: string): void {
    this.statusLine = line;
    this.isStatusShown = true;
    this.cancelStatus?.();
    this.cancelStatus = this.actions.after(statusMilliseconds, () => {
      this.isStatusShown = false;
    });
  }
}

const panelContext = Symbol("panel");

export function providePanel(panel: Panel): void {
  setContext(panelContext, panel);
}

export function usePanel(): Panel {
  const panel = getContext<Panel | undefined>(panelContext);
  if (panel === undefined) {
    throw new Error("no panel was provided to this component");
  }

  return panel;
}
