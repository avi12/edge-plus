import { computeDeclarations, XamlNamespaces } from "./cascade.ts";
import type { AtRuleBlock, Block, RuleBlock } from "./parser.ts";
import { BlockKind, CssSyntaxError, parseCss } from "./parser.ts";
import { lowerDeclarations } from "./properties.ts";
import type { LowerOptions } from "./reader.ts";
import type { ResourceCatalogue } from "./resources.ts";
import { hasStyle, ResourceKind, resolveVariable, resourceNameOf } from "./resources.ts";
import { parseSelectorList } from "./selectors.ts";
import { expandDeclaration } from "./shorthands.ts";
import type { PositionMapper, SourceMapV3 } from "./source-map.ts";
import { createPositionMapper } from "./source-map.ts";
import type { Declaration, Diagnostic, Rule, Severity, SourcePosition, StyleSheetData, Translation } from "./types.ts";
import { parseColor, parseLength, parseNumber } from "./values.ts";

// Svelte's compiled CSS for one component, translated: its rules become data
// the runtime matches against elements, its custom properties (`--accent`)
// become a XAML ResourceDictionary, and everything WinUI cannot say is a
// diagnostic with the line the author wrote.

export interface TranslateInput {
  // Names the sheet and prefixes its diagnostics.
  file: string;
  // `css.code` of Svelte's compile result (css: "external").
  css: string;
  map?: SourceMapV3;
  resources: ResourceCatalogue;
}

const ColorScheme = {
  Default: "Default",
  Dark: "Dark",
  Light: "Light"
} as const;
type ColorScheme = (typeof ColorScheme)[keyof typeof ColorScheme];

const schemeQuery = /^\(\s*prefers-color-scheme\s*:\s*(dark|light)\s*\)$/iu;

interface Variable {
  name: string;
  value: string;
  scheme: ColorScheme;
  at: SourcePosition;
}

interface Translator {
  input: TranslateInput;
  mapPosition: PositionMapper;
  diagnostics: Diagnostic[];
  variables: Variable[];
  rules: Rule[];
  nextOrder: number;
}

function report(translator: Translator, severity: Severity, message: string, at: SourcePosition): void {
  translator.diagnostics.push({
    severity,
    message,
    file: translator.input.file,
    at
  });
}

function reportError(translator: Translator, message: string, generatedAt: SourcePosition): void {
  report(translator, "error", message, translator.mapPosition(generatedAt));
}

function schemeOf(translator: Translator, block: AtRuleBlock): ColorScheme | null {
  const match = block.name === "media" ? schemeQuery.exec(block.prelude) : null;
  if (match === null) {
    const what = block.name === "media" ? `@media ${block.prelude}` : `@${block.name}`;
    reportError(translator, `${what} has no WinUI meaning (only @media (prefers-color-scheme: dark) and (light) do, for custom properties)`, block.at);
    return null;
  }

  return match[1]?.toLowerCase() === "dark" ? ColorScheme.Dark : ColorScheme.Light;
}

function originalDeclaration(translator: Translator, declaration: Declaration): Declaration {
  return {
    ...declaration,
    at: translator.mapPosition(declaration.at)
  };
}

function addRuleBlock(translator: Translator, block: RuleBlock, scheme: ColorScheme): void {
  for (const at of block.nested) {
    reportError(translator, "nested rules have no WinUI meaning (write each selector at the top level)", at);
  }

  const results = parseSelectorList(block.selector);
  for (const result of results) {
    if (!result.isValid) {
      reportError(translator, `selector "${block.selector}": ${result.reason}`, block.at);
      continue;
    }

    const { selector } = result;
    const declarations = block.declarations.map(declaration => originalDeclaration(translator, declaration));
    const customProperties = declarations.filter(declaration => declaration.property.startsWith("--"));
    const properties = declarations.filter(declaration => !declaration.property.startsWith("--"));
    for (const custom of customProperties) {
      translator.variables.push({
        name: custom.property,
        value: custom.value,
        scheme,
        at: custom.at
      });
    }

    if (selector.isRoot) {
      for (const property of properties) {
        translator.diagnostics.push({
          severity: "error",
          message: `${property.property} has no WinUI meaning (:root styles no element; it can only declare custom properties)`,
          file: translator.input.file,
          at: property.at
        });
      }

      continue;
    }

    const isSchemeConditional = scheme !== ColorScheme.Default;
    if (isSchemeConditional) {
      for (const entry of [...properties, ...customProperties]) {
        translator.diagnostics.push({
          severity: "error",
          message: `${entry.property} has no WinUI meaning here (a Style cannot depend on the color scheme: set the color through var(--a-winui-brush) or declare a custom property on :root in the @media block)`,
          file: translator.input.file,
          at: entry.at
        });
      }

      continue;
    }

    translator.rules.push({
      elementType: selector.elementType,
      classes: selector.classes,
      state: selector.state,
      specificity: selector.specificity,
      order: translator.nextOrder,
      declarations: properties
    });
    translator.nextOrder++;
  }
}

function addBlocks(translator: Translator, blocks: Block[], scheme: ColorScheme): void {
  for (const block of blocks) {
    if (block.kind === BlockKind.Rule) {
      addRuleBlock(translator, block, scheme);
      continue;
    }

    const isNestedMedia = scheme !== ColorScheme.Default;
    const inner = isNestedMedia ? null : schemeOf(translator, block);
    if (isNestedMedia) {
      reportError(translator, `@${block.name} inside @media has no WinUI meaning`, block.at);
    } else if (inner !== null && block.children !== null) {
      addBlocks(translator, block.children, inner);
    } else if (inner !== null) {
      reportError(translator, `@${block.name} has no WinUI meaning`, block.at);
    }
  }
}

interface VariableResources {
  // A XAML ResourceDictionary, or null when there are no variables.
  xaml: string | null;
  kinds: Map<string, ResourceKind>;
}

function variableEntries(variable: Variable, reportProblem: (message: string) => void): { key: string; xaml: string; kind: ResourceKind }[] {
  const key = resourceNameOf(variable.name);
  const color = parseColor(variable.value);
  if (color !== null) {
    return [
      {
        key,
        xaml: `<Color x:Key="${key}">${color}</Color>`,
        kind: ResourceKind.Color
      },
      {
        key: `${key}Brush`,
        xaml: `<SolidColorBrush x:Key="${key}Brush" Color="{StaticResource ${key}}"/>`,
        kind: ResourceKind.Brush
      }
    ];
  }

  const length = parseLength(variable.value) ?? parseNumber(variable.value);
  if (length !== null) {
    return [{
      key,
      xaml: `<x:Double x:Key="${key}">${length}</x:Double>`,
      kind: ResourceKind.Double
    }];
  }

  reportProblem(`${variable.name}: expected a color or a length in px, found "${variable.value}"`);
  return [];
}

function buildVariableResources(translator: Translator): VariableResources {
  const entriesByScheme = new Map<ColorScheme, Map<string, string>>();
  const kinds = new Map<string, ResourceKind>();
  for (const variable of translator.variables) {
    const entries = variableEntries(variable, message => translator.diagnostics.push({
      severity: "error",
      message,
      file: translator.input.file,
      at: variable.at
    }));
    const forScheme = entriesByScheme.get(variable.scheme) ?? new Map<string, string>();
    for (const entry of entries) {
      forScheme.set(entry.key, entry.xaml);
      kinds.set(entry.key, entry.kind);
    }

    entriesByScheme.set(variable.scheme, forScheme);
  }

  const lightNames = new Set(translator.variables.filter(variable => variable.scheme !== ColorScheme.Dark).map(variable => variable.name));
  const reportedNames = new Set<string>();
  for (const variable of translator.variables) {
    const isDarkOnly = variable.scheme === ColorScheme.Dark && !lightNames.has(variable.name) && !reportedNames.has(variable.name);
    if (isDarkOnly) {
      reportedNames.add(variable.name);
      translator.diagnostics.push({
        severity: "error",
        message: `${variable.name} is declared for dark only; declare it outside @media too (the light value)`,
        file: translator.input.file,
        at: variable.at
      });
    }
  }

  if (entriesByScheme.size === 0) {
    return {
      xaml: null,
      kinds
    };
  }

  const dictionaries = [ColorScheme.Default, ColorScheme.Light, ColorScheme.Dark].flatMap(scheme => {
    const entries = entriesByScheme.get(scheme);
    if (entries === undefined) {
      return [];
    }

    return [`<ResourceDictionary x:Key="${scheme}">\n${[...entries.values()].join("\n")}\n</ResourceDictionary>`];
  });
  return {
    xaml: `<ResourceDictionary ${XamlNamespaces}>\n<ResourceDictionary.ThemeDictionaries>\n${dictionaries.join("\n")}\n</ResourceDictionary.ThemeDictionaries>\n</ResourceDictionary>`,
    kinds
  };
}

// Lowering every rule alone finds what has no WinUI meaning before the app runs.
function validateRules(translator: Translator, catalogue: ResourceCatalogue): void {
  for (const rule of translator.rules) {
    const options: LowerOptions = {
      elementType: rule.elementType,
      resolveVariable: (name, kind) => resolveVariable(catalogue, name, kind),
      hasStyle: key => hasStyle(catalogue, key),
      report: (severity, message, at) => report(translator, severity, message, at)
    };
    const expanded = rule.declarations.flatMap(declaration => expandDeclaration(declaration, options));
    rule.declarations = expanded;
    lowerDeclarations(computeDeclarations([rule], []), options);
  }
}

export function translateStyleSheet(input: TranslateInput): Translation {
  const translator: Translator = {
    input,
    mapPosition: createPositionMapper(input.map),
    diagnostics: [],
    variables: [],
    rules: [],
    nextOrder: 0
  };
  try {
    addBlocks(translator, parseCss(input.css), ColorScheme.Default);
  } catch (error) {
    if (!(error instanceof CssSyntaxError)) {
      throw error;
    }

    reportError(translator, error.message, error.at);
  }

  const variables = buildVariableResources(translator);
  validateRules(translator, input.resources.extend(variables.kinds));
  return {
    sheet: {
      id: input.file,
      rules: translator.rules,
      resourcesXaml: variables.xaml
    } satisfies StyleSheetData,
    diagnostics: translator.diagnostics
  };
}

export function formatDiagnostic(diagnostic: Diagnostic): string {
  const prefix = `${diagnostic.file}:${diagnostic.at.line}:${diagnostic.at.column}:`;
  return diagnostic.severity === "error" ? `${prefix} ${diagnostic.message}` : `${prefix} warning: ${diagnostic.message}`;
}

export function errorsOf(translation: Translation): Diagnostic[] {
  return translation.diagnostics.filter(diagnostic => diagnostic.severity === "error");
}
