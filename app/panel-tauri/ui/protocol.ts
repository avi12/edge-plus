// The messages between the core (Rust) and this window, one JSON object per
// line on the window's stdin and stdout. Every sentence the window shows comes
// from the core in these messages.

export interface CardWording {
  title: string;
  description: string;
  // A Segoe Fluent Icons code point.
  glyph: number;
}

export interface ThemeRow {
  id: string;
  name: string;
  badges: string[];
  removeLabel: string;
}

export interface ThemeOption {
  id: string;
  label: string;
}

export interface ThemingLabels {
  storeLinkPlaceholder: string;
  add: string;
  addLocal: string;
  pickFile: string;
  pickFolder: string;
  whenLight: string;
  whenDark: string;
  themeListName: string;
  reorderHelp: string;
}

export interface ThemingView {
  labels: ThemingLabels;
  options: ThemeOption[];
  light: string;
  dark: string;
  own: ThemeRow[];
  edge: ThemeRow[];
  // Shown in place of both lists while there are no themes.
  empty: string | null;
}

export interface InjectView {
  wording: CardWording;
  button: string;
  isEnabled: boolean;
}

// The card the Inject card appears under, and the offer it shows (null while
// none is offered).
export interface InjectSlot {
  offer: InjectView | null;
}

export interface CardView {
  key: string;
  wording: CardWording;
  isOn: boolean;
  isLocked: boolean;
  note: string;
  theming: ThemingView | null;
  inject: InjectSlot | null;
}

export interface SectionView {
  name: string;
  cards: CardView[];
}

export interface UpdateView {
  wording: CardWording;
  button: string;
  restarting: string;
}

export interface FooterView {
  by: string;
  author: string;
  authorUri: string;
  version: string;
}

export interface View {
  productName: string;
  openEdgeHint: string | null;
  buildNotice: string;
  sections: SectionView[];
  update: UpdateView | null;
  footer: FooterView;
  dropHint: string;
}

export const ColorMode = {
  Light: "light",
  Dark: "dark"
} as const;
export type ColorMode = (typeof ColorMode)[keyof typeof ColorMode];

export type Action =
  | {
    kind: "setSwitch";
    key: string;
    isOn: boolean;
  }
  | { kind: "inject" }
  | { kind: "restartToUpdate" }
  | {
    kind: "addStoreTheme";
    url: string;
  }
  | {
    kind: "addLocalTheme";
    path: string;
  }
  | {
    kind: "removeTheme";
    id: string;
  }
  | {
    kind: "reorderThemes";
    ids: string[];
  }
  | {
    kind: "assignTheme";
    mode: ColorMode;
    id: string;
  };

export interface Bounds {
  x: number;
  y: number;
  width: number;
  height: number;
}

export type CoreMessage =
  // Before the first view, on a dev swap: where the window it replaces was.
  | {
    type: "place";
    bounds: Bounds;
  }
  | {
    type: "view";
    view: View;
  }
  | {
    type: "reply";
    id: number;
    error: string | null;
  }
  | {
    type: "notice";
    text: string;
  }
  | {
    type: "status";
    line: string;
  }
  | { type: "bringBack" }
  | { type: "close" };

export type UiMessage =
  | { type: "refresh" }
  | {
    type: "action";
    id: number;
    action: Action;
  }
  | {
    type: "bounds";
    bounds: Bounds;
  };
