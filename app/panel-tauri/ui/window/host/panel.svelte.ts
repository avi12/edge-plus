import type { Action } from "#src/protocol.g.ts";
import { getContext, setContext } from "svelte";
import type { WindowHandle } from "winui-svelte/components";
import { startTimer } from "winui-svelte/window";

const STATUS_MILLISECONDS = 4000;

/** Runs an action in the core; resolves to why it failed, or null. */
export type RunAction = (action: Action) => Promise<string | null>;

/**
 * The page's own state and the actions its components run, shared down the
 * tree through context. The View is not here: the page reads it from the
 * host's state.
 */
export class Panel {
  /** The error of an action that failed, shown until the user closes it. */
  actionNotice = $state("");

  /** While files are dragged over the window. */
  isVeilShown = $state(false);

  /** What the core says in the title bar, for a moment. */
  statusLine = $state("");
  isStatusShown = $state(false);

  /** The open window, which owns the pickers. */
  window = $state.raw<WindowHandle | undefined>();
  readonly run: RunAction;
  private stopStatus: (() => void) | null = null;

  constructor(run: RunAction) {
    this.run = run;
  }

  report(error: string | null) {
    const isFailure = error !== null;
    if (isFailure) {
      this.actionNotice = error;
    }
  }

  showStatus(line: string) {
    this.statusLine = line;
    this.isStatusShown = true;
    this.stopStatus?.();
    this.stopStatus = startTimer({
      milliseconds: STATUS_MILLISECONDS,
      isRepeating: false,
      onTick: () => {
        this.isStatusShown = false;
      }
    });
  }
}

const panelContext = Symbol("panel");

/** Shares the panel with every component below the caller. */
export function providePanel(panel: Panel) {
  setContext(panelContext, panel);
}

/** The panel the page provided. */
export function usePanel() {
  const panel = getContext<Panel | undefined>(panelContext);
  const isMissing = panel === undefined;
  if (isMissing) {
    throw new Error("no panel was provided to this component");
  }

  return panel;
}

/** Runs an action and shows why the core refused it, if it did. */
export async function runAndReport({ panel, action }: {
  panel: Panel;
  action: Action;
}) {
  panel.report(await panel.run(action));
}
