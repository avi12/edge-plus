import { ResourceKind } from "../../src/css/runtime/resources.ts";
import { readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { z } from "zod";

// Writes src/css/winui-resources.g.json: every resource key WinUI's generic.xaml
// defines (colors, brushes, thicknesses, radii, doubles, styles...), by kind,
// so the CSS translation can check a `var(--name)` against what WinUI really
// has, at build time and with no WinUI installed.
//
//   node scripts/css/export-winui-resources.ts [path/to/generic.xaml]
//
// The default is the Windows App SDK WinUI package that app/panel-tauri's
// .winapp/winmds.lock.json names, from the NuGet cache it lists.

const sourceFolder = import.meta.dirname;
const outputFile = path.join(sourceFolder, "..", "..", "src", "css", "winui-resources.g.json");
const lockFile = path.join(sourceFolder, "..", "..", "..", "..", "app", "panel-tauri", ".winapp", "winmds.lock.json");
const winUiPackage = "Microsoft.WindowsAppSDK.WinUI";
const genericXamlInPackage = path.join("lib", "native", "Microsoft.UI", "Themes", "generic.xaml");

// The tag a resource is declared with, and the kind that makes it.
const kindOfTag = new Map<string, ResourceKind>([
  ["SolidColorBrush", ResourceKind.Brush],
  ["media:SolidColorBrush", ResourceKind.Brush],
  ["AcrylicBrush", ResourceKind.Brush],
  ["LinearGradientBrush", ResourceKind.Brush],
  ["Color", ResourceKind.Color],
  ["Thickness", ResourceKind.Thickness],
  ["x:Double", ResourceKind.Double],
  ["CornerRadius", ResourceKind.CornerRadius],
  ["FontWeight", ResourceKind.FontWeight],
  ["FontFamily", ResourceKind.FontFamily],
  ["x:String", ResourceKind.String],
  ["x:Boolean", ResourceKind.Boolean],
  ["GridLength", ResourceKind.GridLength],
  ["StaticResource", ResourceKind.Alias],
  ["Style", ResourceKind.Style]
]);

// Colors the framework provides to every app, which generic.xaml only refers to.
const frameworkColors = [
  "SystemAccentColor",
  "SystemAccentColorLight1",
  "SystemAccentColorLight2",
  "SystemAccentColorLight3",
  "SystemAccentColorDark1",
  "SystemAccentColorDark2",
  "SystemAccentColorDark3",
  "SystemColorWindowTextColor",
  "SystemColorWindowColor",
  "SystemColorGrayTextColor",
  "SystemColorHighlightTextColor",
  "SystemColorHighlightColor",
  "SystemColorButtonFaceColor",
  "SystemColorButtonTextColor",
  "SystemColorHotlightColor",
  "SystemColorDisabledTextColor"
];

// Code-unit order, so the file does not change with the machine's locale.
function compareKeys(left: string, right: string): number {
  if (left === right) {
    return 0;
  }

  return left < right ? -1 : 1;
}

function compareEntries<Value>(left: [string, Value], right: [string, Value]): number {
  return compareKeys(left[0], right[0]);
}

const lockFileSchema = z.object({
  packages: z.array(
    z.object({
      name: z.string(),
      version: z.union([z.string(), z.number()])
    })
  ),
  nuget_cache_dir: z.string()
});

function defaultGenericXaml(): string {
  const parsedLock = lockFileSchema.safeParse(JSON.parse(readFileSync(lockFile, "utf8")));
  if (!parsedLock.success) {
    throw new TypeError(`${lockFile} is not a winapp lock file`);
  }

  const { packages, nuget_cache_dir: cacheFolder } = parsedLock.data;
  const winUi = packages.find(candidate => candidate.name === winUiPackage);
  if (winUi === undefined) {
    throw new Error(`${lockFile} names no ${winUiPackage}`);
  }

  const version = String(winUi.version);
  return path.join(cacheFolder, winUiPackage.toLowerCase(), version, genericXamlInPackage);
}

const xamlFile = process.argv[2] ?? defaultGenericXaml();
const xaml = readFileSync(xamlFile, "utf8");
const byKind = new Map<ResourceKind, Set<string>>();
for (const match of xaml.matchAll(/<([\w:]+)\s+x:Key="([^"]+)"/gu)) {
  const kind = kindOfTag.get(match[1] ?? "");
  const key = match[2];
  const isResource = kind !== undefined && key !== undefined;
  if (isResource) {
    const keys = byKind.get(kind) ?? new Set<string>();
    keys.add(key);
    byKind.set(kind, keys);
  }
}

const colors = byKind.get(ResourceKind.Color) ?? new Set<string>();
for (const color of frameworkColors) {
  colors.add(color);
}

byKind.set(ResourceKind.Color, colors);
const output = Object.fromEntries(
  [...byKind].sort(compareEntries).map(([kind, keys]) => [kind, [...keys].sort(compareKeys)])
);
writeFileSync(
  outputFile, `${JSON.stringify({
    source: `${winUiPackage} ${/([\d.]+)[\\/]lib[\\/]/u.exec(xamlFile)?.[1] ?? "?"}`,
    ...output
  })}\n`
);
process.stdout.write(`${[...byKind].map(([kind, keys]) => `${kind} ${keys.size}`).join(", ")}\n`);
