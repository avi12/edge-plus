import type { Action } from "../protocol.g.ts";
import type { LocalThemeKind, PickLocalTheme, RunAction } from "./theming-card.ts";
import { getContext, setContext } from "svelte";

// What the page asks of the window it is in.
export interface PanelActions {
  run: RunAction;
  pick: PickLocalTheme;
  restartToUpdate(): Promise<string | null>;
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
  private readonly actions: PanelActions;

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
    return this.actions.pick(kind);
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
}

const panelContext = Symbol("panel");

export function providePanel(panel: Panel): void {
  setContext(panelContext, panel);
}

export function usePanel(): Panel {
  const panel: unknown = getContext(panelContext);
  if (!(panel instanceof Panel)) {
    throw new Error("no panel was provided to this component");
  }

  return panel;
}
