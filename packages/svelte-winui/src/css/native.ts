import type { CssNative } from "./runtime.ts";
import { installCssRuntime } from "./runtime.ts";

// The one place the CSS runtime touches WinUI: how a dictionary is loaded and
// merged, a Style looked up and set on a control, a grid's definitions
// replaced, and a control's event subscribed. Given the package's WinUI
// import site (the module namespace), so it names no binding path.

interface ProjectedType<Instance extends object> {
  readonly prototype: Instance;
}

interface DictionaryLike {
  lookup(key: unknown): unknown;
}

interface ListLike {
  append(item: unknown): void;
  clear(): void;
}

export interface CssWinui {
  Application: { current: { resources: { mergedDictionaries: ListLike | null } } };
  ResourceDictionary: ProjectedType<DictionaryLike>;
  Style: ProjectedType<object>;
  XamlReader: { load(xaml: string): unknown };
  PropertyValue: { createString(value: string): unknown };
  projectAs<Instance extends object>(value: unknown, type: ProjectedType<Instance>): Instance;
  ColumnDefinition: new () => object;
  RowDefinition: new () => object;
  GridUnitType: { Auto: number; Pixel: number; Star: number };
}

const listSeparator = ",";
const starSuffix = "*";
const autoLength = "Auto";

function isRecord(value: unknown): value is Record<string, unknown> {
  return (typeof value === "object" || typeof value === "function") && value !== null;
}

function read(target: unknown, key: string): unknown {
  return isRecord(target) ? target[key] : undefined;
}

function isDictionary(value: unknown): value is DictionaryLike {
  return isRecord(value) && typeof value["lookup"] === "function";
}

function isList(value: unknown): value is ListLike {
  return isRecord(value) && typeof value["append"] === "function" && typeof value["clear"] === "function";
}

function isEventMethod(value: unknown): value is (handler: () => void) => unknown {
  return typeof value === "function";
}

// "Auto", "*", "2*" or pixels, as WinUI's GridLength.
function gridLength(winui: CssWinui, text: string): object {
  if (text === autoLength) {
    return {
      value: 1,
      gridUnitType: winui.GridUnitType.Auto
    };
  }

  if (text.endsWith(starSuffix)) {
    const weight = text.slice(0, -starSuffix.length);
    return {
      value: weight === "" ? 1 : Number(weight),
      gridUnitType: winui.GridUnitType.Star
    };
  }

  return {
    value: Number(text),
    gridUnitType: winui.GridUnitType.Pixel
  };
}

export function createCssNative(winui: CssWinui): CssNative {
  return {
    loadDictionary: xaml => winui.projectAs(winui.XamlReader.load(xaml), winui.ResourceDictionary),
    mergeIntoApplication(dictionary) {
      const merged = winui.Application.current.resources.mergedDictionaries;
      merged?.append(dictionary);
    },
    lookupStyle(dictionary, key) {
      if (!isDictionary(dictionary)) {
        throw new TypeError("not a ResourceDictionary");
      }

      return winui.projectAs(dictionary.lookup(winui.PropertyValue.createString(key)), winui.Style);
    },
    setStyle(control, style) {
      Object.assign(control, { style });
    },
    setGridDefinitions(control, target, lengths) {
      const isColumns = target === "columns";
      const definitions = read(control, isColumns ? "columnDefinitions" : "rowDefinitions");
      if (!isList(definitions)) {
        throw new TypeError(`${target} are set on a grid`);
      }

      definitions.clear();
      for (const length of lengths === "" ? [] : lengths.split(listSeparator)) {
        const definition = isColumns ? new winui.ColumnDefinition() : new winui.RowDefinition();
        Object.assign(definition, { [isColumns ? "width" : "height"]: gridLength(winui, length.trim()) });
        definitions.append(definition);
      }
    },
    subscribe(control, event, handler) {
      const subscribe = read(control, `on${event}`);
      if (!isEventMethod(subscribe)) {
        return null;
      }

      const unsubscribe = subscribe.call(control, handler);
      return typeof unsubscribe === "function" ? () => unsubscribe() : null;
    },
    read
  };
}

// The renderer's single call, after Application.create: `installCss(winui)`.
export function installCss(winui: CssWinui): void {
  installCssRuntime(createCssNative(winui));
}
