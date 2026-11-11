import type { Action } from "#src/protocol.g.ts";
import { getContext, setContext } from "svelte";
import type { WindowHandle } from "winui-svelte/components";
import type { DragEvent } from "winui-svelte/data-transfer";
import { startTimer } from "winui-svelte/window";

const STATUS_MILLISECONDS = 4000;
const USED_THEME_WINDOW_MILLISECONDS = 10_000;

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

  /**
   * Why a theme could not be added, shown in the Theming card until an attempt succeeds. An attempt leaves it
   * as it is until its own result is in, so a new reason replaces the text in place instead of the line
   * leaving and returning, which moved the list twice.
   */
  themeError = $state("");

  /** A theme is being added to be used at once: the row that appears for it waits for Edge to wear it. */
  isUsingNewTheme = $state(false);

  /** Said when an add starts, so the row that comes of it knows; forgotten once it would have come. */
  expectUsedTheme(isUsed: boolean) {
    this.isUsingNewTheme = isUsed;

    if (isUsed) {
      setTimeout(() => (this.isUsingNewTheme = false), USED_THEME_WINDOW_MILLISECONDS);
    }
  }

  /** Adds every file and folder of a drop as a local theme. */
  async addDroppedThemes(event: DragEvent) {
    this.expectUsedTheme(event.shiftKey);
    const failures: string[] = [];
    for (const file of event.dataTransfer.files) {
      const failure = await this.run({
        kind: "addLocalTheme",
        path: file.path,
        shouldUseNow: event.shiftKey
      });
      if (failure !== null) {
        failures.push(`${file.name}: ${failure}`);
      }
    }

    this.themeError = failures.join("\n");
  }

  /** Tells the user that a drop held nothing that can be a theme. */
  rejectDroppedThemes() {
    this.themeError = "That is not a theme: drop a folder with a manifest.json, a .zip or a .crx";
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
