import { execFileSync } from "node:child_process";
import {
  copyFileSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  writeFileSync
} from "node:fs";
import { createRequire } from "node:module";
import path from "node:path";

// Builds dist/, what is published: the TypeScript compiled to JavaScript with
// its declarations (tsc), and the Svelte pieces shipped as source, the way
// Svelte libraries are, for the app's own Vite to compile with the custom
// renderer:
//
//   .svelte        the components, their relative imports pointing at the
//                  compiled .js files;
//   .d.svelte.ts   the types of the generated components, for TypeScript
//                  without the Svelte language tools;
//   *.svelte.js    modules with runes, which the app's Svelte plugin compiles;
//   .json          data the code reads next to itself.
//
// (@sveltejs/package, the usual tool, hands a *.d.svelte.ts file to TypeScript
// as source and fails, so tsc and a copy do it.)

const packageRoot = path.join(import.meta.dirname, "..", "..");
const repositoryRoot = path.join(packageRoot, "..", "..");
const sourceFolder = path.join(packageRoot, "src");
const outputFolder = path.join(packageRoot, "dist");
const copiedExtensions = [".svelte", ".json"];
const declarationSuffix = ".d.svelte.ts";
const legalFiles = ["LICENSE", "NOTICE"];
// `from "./x.ts"` and `import("./x.ts")` in a .svelte script.
const relativeTsImport = /(["'])(\.{1,2}\/[^"']*?)\.ts\1/gu;

function filesIn(folder: string): string[] {
  return readdirSync(folder, {
    recursive: true,
    withFileTypes: true
  })
    .filter(entry => entry.isFile())
    .map(entry => path.join(entry.parentPath, entry.name));
}

function isShipped(file: string): boolean {
  const isDeclaration = file.endsWith(declarationSuffix);
  const isCopied = copiedExtensions.includes(path.extname(file));
  const isTestSupport = /\.fixture\.svelte$|\.test\./u.test(file);
  return (isDeclaration || isCopied) && !isTestSupport;
}

// A .svelte file's imports of .ts modules, as the .js they are compiled to; a
// .d.svelte.ts import is a type and stays.
function withCompiledImports(source: string): string {
  return source.replaceAll(relativeTsImport, (whole, quote: string, target: string) => {
    return target.endsWith(".d.svelte") ? whole : `${quote}${target}.js${quote}`;
  });
}

function copyShipped(): void {
  for (const file of filesIn(sourceFolder).filter(isShipped)) {
    const target = path.join(outputFolder, path.relative(sourceFolder, file));
    mkdirSync(path.dirname(target), { recursive: true });

    if (file.endsWith(".svelte")) {
      writeFileSync(target, withCompiledImports(readFileSync(file, "utf8")));
    } else {
      copyFileSync(file, target);
    }
  }
}

function compile(): void {
  const tsc = createRequire(import.meta.url).resolve("typescript/bin/tsc");
  execFileSync(process.execPath, [tsc, "-p", path.join(packageRoot, "tsconfig.build.json")], {
    cwd: packageRoot,
    stdio: "inherit"
  });
}

// The license text travels with the package; the repository holds the one.
function copyLegalFiles(): void {
  for (const name of legalFiles) {
    copyFileSync(path.join(repositoryRoot, name), path.join(packageRoot, name));
  }
}

rmSync(outputFolder, {
  recursive: true,
  force: true
});
compile();
copyShipped();
copyLegalFiles();
process.stdout.write(`dist: ${filesIn(outputFolder).length} files\n`);
