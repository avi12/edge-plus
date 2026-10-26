// The words every part of the CSS translation shares. Nothing here imports
// WinUI: the translation runs in Node at build time and in tests.

export interface SourcePosition {
  line: number;
  column: number;
}

export type Severity = "error" | "warning";

export interface Diagnostic {
  severity: Severity;
  message: string;
  file: string;
  at: SourcePosition;
}

export interface Declaration {
  // Lower case; a custom property keeps its `--` prefix.
  property: string;
  value: string;
  isImportant: boolean;
  at: SourcePosition;
  // For a value that is a `var(--name)`: the WinUI resource key it resolved
  // to at build time, spelled exactly as WinUI spells it.
  resource?: string;
}

// The states WinUI's controls report through events. Each is a flag the
// runtime keeps per element and matches against a rule's `state`.
export const PseudoState = {
  Hover: "hover",
  Active: "active",
  Focus: "focus",
  FocusVisible: "focus-visible",
  Disabled: "disabled",
  Checked: "checked"
} as const;
export type PseudoState = (typeof PseudoState)[keyof typeof PseudoState];

// One compound selector with its declarations. A selector list is several
// rules; a rule is data, so a sheet is JSON.
export interface Rule {
  // "text-block", or null when the selector names no element type.
  elementType: string | null;
  classes: string[];
  state: PseudoState | null;
  // Classes plus pseudo-class plus one for a type, as in CSS (`:where()` is 0).
  specificity: number;
  // Source order across the whole sheet.
  order: number;
  declarations: Declaration[];
}

export interface StyleSheetData {
  // The component's file: registering the same id again replaces the sheet.
  id: string;
  rules: Rule[];
  // A XAML ResourceDictionary of the sheet's custom properties, or null.
  resourcesXaml: string | null;
}

export interface Translation {
  sheet: StyleSheetData;
  diagnostics: Diagnostic[];
}
