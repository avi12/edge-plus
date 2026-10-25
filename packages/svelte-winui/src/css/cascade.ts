import type { LoweredStyle } from "./properties.ts";
import type { VariableResolution } from "./resources.ts";
import type { Declaration, PseudoState, Rule } from "./types.ts";

// CSS's cascade for one element: which rules apply to it (its type, classes and
// current states), in what order, and the XAML Style that results. Pure: the
// runtime hands the result to XamlReader.

export interface RankedRule {
  rule: Rule;
  // Names the rule across sheets, for caching what it styles.
  id: string;
  // The order across sheets: a rule registered later ranks higher.
  rank: number;
}

export interface ElementFacts {
  elementType: string;
  classes: ReadonlySet<string>;
  states: ReadonlySet<PseudoState>;
}

function ruleApplies(rule: Rule, facts: ElementFacts, isStateIgnored: boolean): boolean {
  const isTypeMatch = rule.elementType === null || rule.elementType === facts.elementType;
  const hasClasses = rule.classes.every(name => facts.classes.has(name));
  const isStateActive = rule.state === null || isStateIgnored || facts.states.has(rule.state);
  return isTypeMatch && hasClasses && isStateActive;
}

// The rules that style the element, lowest priority first. With
// `isStateIgnored`, a state rule counts whether or not its state is active:
// that is how the runtime learns which events an element needs.
export function matchingRanked(rules: readonly RankedRule[], facts: ElementFacts, isStateIgnored = false): RankedRule[] {
  return rules
    .filter(ranked => ruleApplies(ranked.rule, facts, isStateIgnored))
    .sort((first, second) => first.rule.specificity - second.rule.specificity || first.rank - second.rank);
}

export function matchingRules(rules: readonly RankedRule[], facts: ElementFacts, isStateIgnored = false): Rule[] {
  return matchingRanked(rules, facts, isStateIgnored).map(ranked => ranked.rule);
}

// Later declarations win; `!important` ones win over every other. The
// element's own `style` declarations come last.
export function computeDeclarations(matched: readonly Rule[], inline: readonly Declaration[]): Map<string, Declaration> {
  const normal = new Map<string, Declaration>();
  const important = new Map<string, Declaration>();
  const declarations = [...matched.flatMap(rule => rule.declarations), ...inline];
  for (const declaration of declarations) {
    (declaration.isImportant ? important : normal).set(declaration.property, declaration);
  }

  return new Map([...normal, ...important]);
}

// The runtime does not know WinUI's resource names: the build wrote the key
// each var() resolved to on its declaration.
export function resolveBuiltVariable(declaration: Declaration, name: string): VariableResolution {
  if (declaration.resource === undefined) {
    return {
      isFound: false,
      reason: `${name} was not resolved at build time (var() works in a <style> block, not in a style attribute)`
    };
  }

  return {
    isFound: true,
    key: declaration.resource
  };
}

const xmlEscapes = new Map([
  ["&", "&amp;"],
  ["<", "&lt;"],
  [">", "&gt;"],
  ["\"", "&quot;"]
]);

function escapeAttribute(value: string): string {
  return value.replace(/[&<>"]/gu, character => xmlEscapes.get(character) ?? character);
}

// "stack-panel" as StackPanel, the XAML name of the control.
export function xamlTypeName(elementType: string): string {
  return elementType.split("-").map(word => `${word.charAt(0).toUpperCase()}${word.slice(1)}`).join("");
}

export const XamlNamespaces = `xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"`;

export function styleXaml(key: string, elementType: string, lowered: LoweredStyle): string {
  const basedOn = lowered.basedOn === null ? "" : ` BasedOn="{StaticResource ${lowered.basedOn}}"`;
  const setters = lowered.setters.map(entry => {
    if (entry.kind === "setter") {
      return `  <Setter Property="${entry.property}" Value="${escapeAttribute(entry.value)}"/>`;
    }

    return `  <Setter Property="${entry.property}"><Setter.Value>${entry.xaml}</Setter.Value></Setter>`;
  });
  return [
    `<Style x:Key="${escapeAttribute(key)}" TargetType="${xamlTypeName(elementType)}"${basedOn}>`,
    ...setters,
    "</Style>"
  ].join("\n");
}

// A dictionary that holds one style, as XamlReader loads it.
export function styleDictionaryXaml(key: string, elementType: string, lowered: LoweredStyle): string {
  return `<ResourceDictionary ${XamlNamespaces}>\n${styleXaml(key, elementType, lowered)}\n</ResourceDictionary>`;
}
