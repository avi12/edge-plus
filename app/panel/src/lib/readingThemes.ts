import { ReadingTheme } from "@/lib/bridge";

// How each reading mode page theme is shown in the panel: its name and the
// colour of its swatch in Edge's Themes grid (learning_tools' theme table, see
// docs/re/reading-mode-theme.md). In the grid's order.

export interface ReadingThemeInfo {
  theme: ReadingTheme;
  label: string;
  color: string;
}

export const READING_THEMES: ReadingThemeInfo[] = [
  {
    theme: ReadingTheme.Sepia,
    label: "Sepia",
    color: "#F9F5E9"
  },
  {
    theme: ReadingTheme.Light,
    label: "Light",
    color: "#FCFCFC"
  },
  {
    theme: ReadingTheme.Grey,
    label: "Grey",
    color: "#E6E6E6"
  },
  {
    theme: ReadingTheme.Blue,
    label: "Blue",
    color: "#87FAFF"
  },
  {
    theme: ReadingTheme.Yellow,
    label: "Yellow",
    color: "#FEFF5C"
  },
  {
    theme: ReadingTheme.Green,
    label: "Green",
    color: "#91FFA6"
  },
  {
    theme: ReadingTheme.Lavender,
    label: "Lavender",
    color: "#D1BFEB"
  },
  {
    theme: ReadingTheme.LightBlue,
    label: "Light blue",
    color: "#B3CAEC"
  },
  {
    theme: ReadingTheme.SkyBlue,
    label: "Sky blue",
    color: "#A3CFE4"
  },
  {
    theme: ReadingTheme.LightGreen,
    label: "Light green",
    color: "#A5DA90"
  },
  {
    theme: ReadingTheme.Black,
    label: "Black",
    color: "#000000"
  },
  {
    theme: ReadingTheme.DarkGrey,
    label: "Dark grey",
    color: "#242424"
  }
];

/** A round colour chip for a picker row, as an inline SVG. */
export function swatchIcon(color: string): string {
  return (
    `<svg viewBox="0 0 16 16"><circle cx="8" cy="8" r="6.5" fill="${color}" ` +
    `stroke="currentColor" stroke-opacity="0.35" stroke-width="1"/></svg>`
  );
}

/** The theme a picker sent back, or undefined for a value that is not one. */
export function readingThemeOf(value: string): ReadingTheme | undefined {
  return READING_THEMES.find(info => info.theme === value)?.theme;
}
