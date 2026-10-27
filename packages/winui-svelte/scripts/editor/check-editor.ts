// Fails when the editor would underline anything: every .svelte file of the
// package and of each --project is opened in the Svelte language server (the
// version VS Code's Svelte extension bundles, pinned, with the patch in
// patches/), and any diagnostic, or an inlay hint that shows a type collapsed
// to any, fails.
//
//   node scripts/editor/check-editor.ts --project ../../app/panel-tauri
//
// svelte-check runs the same plugins, and both parse with the project's Svelte
// for a file a tsconfig covers. They part where none does: the server then
// falls back to the Svelte 4 it bundles, which rejects Svelte 5 markup, while
// svelte-check skips the file. So every .svelte file is in a tsconfig's
// include, and this check catches one that is not.
import { type InlayHint, LanguageServer } from "./language-server.ts";
import { readdirSync } from "node:fs";
import path from "node:path";
import { parseArgs } from "node:util";

const packageFolder = path.resolve(import.meta.dirname, "../..");
const skippedFolders = ["node_modules", "dist", ".winapp", "target"];
const anyTypePattern = /:\s*any\b/;

// Walked by hand, not recursively by readdir: node_modules holds the package's
// link to itself, which a recursive walk follows without end.
function svelteFilesIn(folder: string): string[] {
  const files: string[] = [];
  for (const entry of readdirSync(folder, { withFileTypes: true })) {
    const entryPath = path.join(folder, entry.name);
    const isWalkedFolder = entry.isDirectory() && !skippedFolders.includes(entry.name);
    const isSvelteFile = entry.isFile() && entry.name.endsWith(".svelte");
    if (isWalkedFolder) {
      files.push(...svelteFilesIn(entryPath));
    } else if (isSvelteFile) {
      files.push(entryPath);
    }
  }

  return files;
}

function labelOf(hint: InlayHint): string {
  return typeof hint.label === "string" ? hint.label : hint.label.map(part => part.value).join("");
}

// The folder both the package and the projects sit under: the editor's
// workspace, so each file finds its own tsconfig as it does there.
function commonFolder(folders: string[]): string {
  const [first = packageFolder, ...rest] = folders;
  let common = first;
  for (const folder of rest) {
    while (path.relative(common, folder).startsWith("..")) {
      common = path.dirname(common);
    }
  }

  return common;
}

async function problemsOf(server: LanguageServer, file: string): Promise<string[]> {
  const where = path.relative(process.cwd(), file);
  const problems = server.diagnosticsOf(file).map(diagnostic => {
    const { line, character } = diagnostic.range.start;
    const firstLine = diagnostic.message.split("\n")[0];
    return `${where}:${line + 1}:${character + 1} ${firstLine} (${diagnostic.source ?? ""} ${diagnostic.code ?? ""})`;
  });
  for (const hint of await server.inlayHintsOf(file)) {
    const label = labelOf(hint);
    if (anyTypePattern.test(label)) {
      problems.push(`${where}:${hint.position.line + 1}:${hint.position.character + 1} a type collapsed to any (inlay hint "${label.trim()}")`);
    }
  }

  return problems;
}

const { values } = parseArgs({
  options: {
    project: {
      type: "string",
      multiple: true,
      default: []
    }
  }
});
const folders = [packageFolder, ...values.project.map(project => path.resolve(project))];
const files = folders.flatMap(folder => svelteFilesIn(folder));

const server = new LanguageServer(packageFolder);
const problems: string[] = [];
try {
  await server.start(commonFolder(folders));
  for (const file of files) {
    server.open(file);
  }

  await server.settle(files);
  for (const file of files) {
    problems.push(...await problemsOf(server, file));
  }
} finally {
  server.stop();
}

for (const problem of problems) {
  console.error(problem);
}

console.log(`Editor check: ${files.length} Svelte files, ${problems.length} problems.`);
process.exitCode = problems.length === 0 ? 0 : 1;
