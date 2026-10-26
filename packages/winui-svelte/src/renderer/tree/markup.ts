// The words of the window's .svelte markup, read without WinUI: a kebab-case
// tag or attribute as the class or property it names, and the quoted values
// XAML spells as text (a Thickness, a grid length, a number).

const wordSeparator = "-";
const listSeparator = ",";
const autoLength = "Auto";
const starSuffix = "*";

export const GridUnit = {
  Auto: "auto",
  Pixel: "pixel",
  Star: "star"
} as const;

export type GridUnit = typeof GridUnit[keyof typeof GridUnit];

export interface GridLength {
  unit: GridUnit;
  value: number;
}

export function pascalCase(name: string): string {
  return name.split(wordSeparator).map(word => `${word.charAt(0).toUpperCase()}${word.slice(1)}`).join("");
}

export function camelCase(name: string): string {
  const pascal = pascalCase(name);
  return `${pascal.charAt(0).toLowerCase()}${pascal.slice(1)}`;
}

// A class name as its tag: TextBlock is <text-block>.
export function kebabCase(name: string): string {
  return name.replace(/(?<=[a-z0-9])(?=[A-Z])/gu, wordSeparator).toLowerCase();
}

export function isNumeric(text: string): boolean {
  return text.trim() !== "" && Number.isFinite(Number(text));
}

export function splitList(text: string): string[] {
  return text.split(listSeparator).map(item => item.trim());
}

// "8" for every side, "8,4" for left-right and top-bottom, or four sides, as
// in XAML: left, top, right, bottom.
export function thicknessSides(text: string): [number, number, number, number] {
  const [left = 0, top = left, right = left, bottom = top] = splitList(text).map(Number);
  return [left, top, right, bottom];
}

// "Auto", "*", "2*" or pixels, as in XAML.
export function gridLength(text: string): GridLength {
  if (text === autoLength) {
    return {
      unit: GridUnit.Auto,
      value: 1
    };
  }

  return text.endsWith(starSuffix) ? starLength(text.slice(0, -starSuffix.length)) : {
    unit: GridUnit.Pixel,
    value: Number(text)
  };
}

// "*" alone weighs 1.
function starLength(weight: string): GridLength {
  return {
    unit: GridUnit.Star,
    value: weight === "" ? 1 : Number(weight)
  };
}
