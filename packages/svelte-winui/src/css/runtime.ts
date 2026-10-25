import type { RankedRule } from "./cascade.ts";
import { computeDeclarations, matchingRanked, resolveBuiltVariable, styleDictionaryXaml } from "./cascade.ts";
import { parseInlineStyle } from "./inline.ts";
import type { LoweredStyle } from "./properties.ts";
import { lowerDeclarations } from "./properties.ts";
import type { LowerOptions } from "./reader.ts";
import type { Declaration, PseudoState, StyleSheetData } from "./types.ts";

// Applies the translated stylesheets to the controls the renderer makes: for
// each element it matches the rules by type, classes and states (CSS's
// cascade), builds the WinUI Style the result calls for, and keeps it in step
// as classes, states and the sheets themselves change (hot reload).
//
// Nothing here imports WinUI: it reaches the controls through `CssNative`
// (native.ts is the one implementation), so the logic runs under node --test.

export interface CssNative {
  // XamlReader.load of a ResourceDictionary.
  loadDictionary(xaml: string): object;
  mergeIntoApplication(dictionary: object): void;
  lookupStyle(dictionary: object, key: string): object;
  setStyle(control: object, style: object | null): void;
  // A grid's columns or rows, from "Auto,*,2*,120" (empty clears them).
  setGridDefinitions(control: object, target: "columns" | "rows", lengths: string): void;
  // Calls `handler` on a WinUI event (PointerEntered...); null if the control has no such event.
  subscribe(control: object, event: string, handler: () => void): (() => void) | null;
  read(control: object, property: string): unknown;
}

export interface StyledElement {
  // The whole class attribute, on every change.
  setClasses(classNames: string): void;
  // The whole style attribute, which Svelte keeps as one string whether it
  // was written as style="..." or as style:name={value}; null removes it.
  setInlineStyle(cssText: string | null): void;
  dispose(): void;
}

const rankStride = 100_000;
const keyboardFocus = 2;
const cacheLimit = 2000;
const classSeparator = /\s+/u;

type ReadProperty = (property: string) => unknown;

interface StateBinding {
  event: string;
  isActive(read: ReadProperty): boolean;
}

interface StateSupport {
  bindings: StateBinding[];
  initially?(read: ReadProperty): boolean;
}

function isAlways(): boolean {
  return true;
}

function isNever(): boolean {
  return false;
}

// The WinUI events that say a control is in a state. A control handles some
// pointer events itself (a Button marks PointerPressed handled), so `:active`
// only works on controls that leave them to the tree.
const stateSupport: ReadonlyMap<PseudoState, StateSupport> = new Map<PseudoState, StateSupport>([
  ["hover", {
    bindings: [
      {
        event: "PointerEntered",
        isActive: isAlways
      },
      {
        event: "PointerExited",
        isActive: isNever
      }
    ]
  }],
  ["active", {
    bindings: [
      {
        event: "PointerPressed",
        isActive: isAlways
      },
      {
        event: "PointerReleased",
        isActive: isNever
      },
      {
        event: "PointerCanceled",
        isActive: isNever
      },
      {
        event: "PointerCaptureLost",
        isActive: isNever
      }
    ]
  }],
  ["focus", {
    bindings: [
      {
        event: "GotFocus",
        isActive: isAlways
      },
      {
        event: "LostFocus",
        isActive: isNever
      }
    ]
  }],
  ["focus-visible", {
    bindings: [
      {
        event: "GotFocus",
        isActive: read => read("focusState") === keyboardFocus
      },
      {
        event: "LostFocus",
        isActive: isNever
      }
    ]
  }],
  ["disabled", {
    bindings: [{
      event: "IsEnabledChanged",
      isActive: read => read("isEnabled") === false
    }],
    initially: read => read("isEnabled") === false
  }],
  ["checked", {
    bindings: [
      {
        event: "Checked",
        isActive: isAlways
      },
      {
        event: "Unchecked",
        isActive: isNever
      },
      {
        event: "Toggled",
        isActive: read => read("isOn") === true
      }
    ],
    initially: read => read("isChecked") === true || read("isOn") === true
  }]
]);

interface BuiltStyle {
  style: object | null;
  writes: LoweredStyle["writes"];
}

interface RegisteredSheet {
  sheet: StyleSheetData;
  // Keeps its place in the cascade when a hot reload replaces it.
  sequence: number;
  // Counts the times the sheet was registered, to tell its rules from the ones
  // it replaced.
  version: number;
}

const sheets = new Map<string, RegisteredSheet>();
const elements = new Set<ElementStyle>();
const builtStyles = new Map<string, BuiltStyle>();
const warned = new Set<string>();
let native: CssNative | null = null;
let rules: RankedRule[] = [];
let nextSequence = 0;
// Each Style is loaded in a dictionary of its own, so one key does for all.
const styleResourceKey = "css-style";
const stylesByXaml = new Map<string, object>();

function warnOnce(message: string): void {
  if (!warned.has(message)) {
    warned.add(message);
    process.stderr.write(`svelte-winui css: ${message}\n`);
  }
}

const idSeparator = "#";

// A rule's id and its sheet's version, to tell it from the rule of the same
// name in the sheet a hot reload replaced.
function versionedId(id: string): string {
  const sheetId = id.slice(0, id.lastIndexOf(idSeparator));
  return `${id}@${sheets.get(sheetId)?.version ?? 0}`;
}

function rebuildRules(): void {
  rules = [...sheets.values()].flatMap(({ sheet, sequence }) => sheet.rules.map(rule => ({
    rule,
    id: `${sheet.id}${idSeparator}${rule.order}`,
    rank: sequence * rankStride + rule.order
  })));
  builtStyles.clear();
}

function mergeResources(registered: RegisteredSheet): void {
  const xaml = registered.sheet.resourcesXaml;
  if (native === null || xaml === null) {
    return;
  }

  native.mergeIntoApplication(native.loadDictionary(xaml));
}

function refreshAll(): void {
  for (const element of elements) {
    element.refresh();
  }
}

// Same id replaces the sheet (hot reload): its custom properties are merged
// again after the old ones, so they win, and every live element re-applies.
export function registerStyleSheet(sheet: StyleSheetData): void {
  const previous = sheets.get(sheet.id);
  const registered: RegisteredSheet = {
    sheet,
    sequence: previous?.sequence ?? nextSequence++,
    version: (previous?.version ?? 0) + 1
  };
  sheets.set(sheet.id, registered);
  rebuildRules();
  mergeResources(registered);
  refreshAll();
}

export function unregisterStyleSheet(id: string): void {
  sheets.delete(id);
  rebuildRules();
  refreshAll();
}

// Once, after Application.create has installed WinUI's own resources: sheets
// registered before it (component modules load first) are merged now.
export function installCssRuntime(implementation: CssNative): void {
  native = implementation;
  for (const registered of sheets.values()) {
    mergeResources(registered);
  }

  builtStyles.clear();
  stylesByXaml.clear();
  refreshAll();
}

function lowerOptions(elementType: string, problems: string[]): LowerOptions {
  return {
    elementType,
    resolveVariable: resolveBuiltVariable,
    // The build checked every -winui-based-on against WinUI's styles.
    hasStyle: isAlways,
    report(severity, message, position) {
      if (severity === "error") {
        problems.push(`${message} (${position.line}:${position.column})`);
      } else {
        warnOnce(message);
      }
    }
  };
}

function buildStyle(elementType: string, matched: RankedRule[], inline: Declaration[]): BuiltStyle {
  if (native === null) {
    throw new Error("installCss has not been called");
  }

  const problems: string[] = [];
  const lowered = lowerDeclarations(
    computeDeclarations(matched.map(ranked => ranked.rule), inline),
    lowerOptions(elementType, problems)
  );
  if (problems.length > 0) {
    const where = matched.map(ranked => ranked.id).join(", ");
    throw new Error(`<${elementType}> cannot take the style of ${where === "" ? "its style attribute" : where}: ${problems.join("; ")}`);
  }

  const isEmpty = lowered.setters.length === 0 && lowered.basedOn === null;
  if (isEmpty) {
    return {
      style: null,
      writes: lowered.writes
    };
  }

  // The same XAML is the same Style object, so a hot reload that left an
  // element's declarations as they were does not set a new one on it.
  const xaml = styleDictionaryXaml(styleResourceKey, elementType, lowered);
  const known = stylesByXaml.get(xaml);
  if (known !== undefined) {
    return {
      style: known,
      writes: lowered.writes
    };
  }

  const style = native.lookupStyle(native.loadDictionary(xaml), styleResourceKey);
  stylesByXaml.set(xaml, style);
  return {
    style,
    writes: lowered.writes
  };
}

function builtFor(key: string, elementType: string, matched: RankedRule[], inline: Declaration[]): BuiltStyle {
  const known = builtStyles.get(key);
  if (known !== undefined) {
    return known;
  }

  if (builtStyles.size >= cacheLimit) {
    builtStyles.clear();
    stylesByXaml.clear();
  }

  const built = buildStyle(elementType, matched, inline);
  builtStyles.set(key, built);
  return built;
}

class ElementStyle implements StyledElement {
  private readonly control: object;
  private readonly elementType: string;
  private classes = new Set<string>();
  private inlineText: string | null = null;
  private readonly states = new Set<PseudoState>();
  private readonly subscriptions = new Map<PseudoState, Array<() => void>>();
  private appliedKey: string | null = null;
  private appliedStyle: object | null = null;
  private appliedWrites = "";
  private isDisposed = false;

  constructor(control: object, elementType: string) {
    this.control = control;
    this.elementType = elementType;
    elements.add(this);
  }

  setClasses(classNames: string): void {
    this.classes = new Set(classNames.split(classSeparator).filter(name => name !== ""));
    this.refresh();
  }

  setInlineStyle(cssText: string | null): void {
    this.inlineText = cssText === null || cssText.trim() === "" ? null : cssText;
    this.refresh();
  }

  dispose(): void {
    this.isDisposed = true;
    elements.delete(this);
    for (const unsubscribers of this.subscriptions.values()) {
      unsubscribers.forEach(unsubscribe => unsubscribe());
    }

    this.subscriptions.clear();
  }

  private inlineDeclarations(): Declaration[] {
    const problems: string[] = [];
    const options = lowerOptions(this.elementType, problems);
    const declarations = this.inlineText === null ? [] : parseInlineStyle(this.inlineText, options);
    if (problems.length > 0) {
      throw new Error(`<${this.elementType}> style: ${problems.join("; ")}`);
    }

    return declarations;
  }

  refresh(): void {
    const currentNative = native;
    if (currentNative === null || this.isDisposed) {
      return;
    }

    const candidates = matchingRanked(rules, {
      elementType: this.elementType,
      classes: this.classes,
      states: this.states
    }, true);
    this.syncStates(new Set(candidates.flatMap(({ rule }) => (rule.state === null ? [] : [rule.state]))));
    const matched = candidates.filter(({ rule }) => rule.state === null || this.states.has(rule.state));
    const inline = this.inlineDeclarations();
    const inlineKey = inline.map(({ property, value }) => `${property}:${value}`).join(";");
    // The key carries each rule's sheet version, so an element is restyled
    // only when a sheet it matches was replaced.
    const key = [this.elementType, ...matched.map(({ id }) => versionedId(id)), inlineKey].join("|");
    if (key === this.appliedKey) {
      return;
    }

    const isUnstyled = matched.length === 0 && inline.length === 0;
    const hadStyle = this.appliedKey !== null;
    if (isUnstyled && !hadStyle) {
      return;
    }

    const built = builtFor(key, this.elementType, matched, inline);
    if (built.style !== this.appliedStyle) {
      currentNative.setStyle(this.control, built.style);
      this.appliedStyle = built.style;
    }

    this.applyWrites(currentNative, built);
    this.appliedKey = isUnstyled ? null : key;
  }

  private applyWrites(currentNative: CssNative, built: BuiltStyle): void {
    const signature = built.writes.map(write => `${write.target}=${write.value}`).join(";");
    if (signature === this.appliedWrites) {
      return;
    }

    for (const write of built.writes) {
      currentNative.setGridDefinitions(this.control, write.target, write.value);
    }

    this.appliedWrites = signature;
  }

  private readProperty: ReadProperty = property => (native === null ? undefined : native.read(this.control, property));

  // Listens for exactly the states some matching rule asks about.
  private syncStates(wanted: Set<PseudoState>): void {
    for (const [state, unsubscribers] of this.subscriptions) {
      if (!wanted.has(state)) {
        unsubscribers.forEach(unsubscribe => unsubscribe());
        this.subscriptions.delete(state);
        this.states.delete(state);
      }
    }

    for (const state of wanted) {
      if (!this.subscriptions.has(state)) {
        this.subscribeTo(state);
      }
    }
  }

  private subscribeTo(state: PseudoState): void {
    const support = stateSupport.get(state);
    if (native === null || support === undefined) {
      return;
    }

    const unsubscribers: Array<() => void> = [];
    for (const binding of support.bindings) {
      const unsubscribe = native.subscribe(this.control, binding.event, () => {
        const isActive = binding.isActive(this.readProperty);
        const isChanged = isActive !== this.states.has(state);
        if (isActive) {
          this.states.add(state);
        } else {
          this.states.delete(state);
        }

        if (isChanged) {
          this.refresh();
        }
      });
      if (unsubscribe !== null) {
        unsubscribers.push(unsubscribe);
      }
    }

    if (unsubscribers.length === 0) {
      warnOnce(`:${state} on <${this.elementType}> does nothing: the control reports none of ${support.bindings.map(binding => binding.event).join(", ")}`);
    }

    this.subscriptions.set(state, unsubscribers);
    if (support.initially?.(this.readProperty) === true) {
      this.states.add(state);
    }
  }
}

// One per element the renderer creates that is given class, style or style:.
export function styledElement(control: object, elementType: string): StyledElement {
  return new ElementStyle(control, elementType);
}
