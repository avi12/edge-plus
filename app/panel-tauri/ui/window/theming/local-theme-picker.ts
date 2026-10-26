import type { LocalThemeKind } from "../cards/theming-card.ts";
import { LocalThemeKind as ThemeKind } from "../cards/theming-card.ts";
import { FileOpenPicker, FolderPicker } from "../winui.ts";
import type { WindowHandle } from "svelte-winui/components";

const themeFileTypes = [".crx", ".zip"];

async function pickFolder(window: WindowHandle): Promise<string | null> {
  const folder = await new FolderPicker(window.appWindow.id).pickSingleFolderAsync();
  return folder?.path ?? null;
}

async function pickFile(window: WindowHandle): Promise<string | null> {
  const picker = new FileOpenPicker(window.appWindow.id);
  for (const fileType of themeFileTypes) {
    picker.fileTypeFilter?.append(fileType);
  }

  const file = await picker.pickSingleFileAsync();
  return file?.path ?? null;
}

// The system's picker for a theme file or folder, owned by the window;
// resolves to null when the user cancels.
export function pickLocalTheme(kind: LocalThemeKind, window: WindowHandle): Promise<string | null> {
  return kind === ThemeKind.Folder ? pickFolder(window) : pickFile(window);
}
