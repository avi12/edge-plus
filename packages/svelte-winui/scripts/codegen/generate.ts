import { packageJsonWithBindings } from "../../src/cli/binding-list.ts";
import {
  bindingModule,
  emitBindings,
  emitComponent,
  emitDeclaration,
  emitIndex,
  emitLevel,
  emitMetadata,
  emitRequiredBindings
} from "./emit.ts";
import { bindingClasses, buildModel, requestedClasses } from "./model.ts";
import type { TypeName } from "./winmd.ts";
import {
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  symlinkSync,
  writeFileSync
} from "node:fs";
import path from "node:path";
import { parseArgs } from "node:util";

// Generates the components from WinUI's metadata:
//
//   node scripts/codegen/generate.ts --project ../../app/panel-tauri [--check]
//
// --project names the folder `winapp restore` was run in: its
// .winapp/winmds.lock.json says where the metadata is, and its package.json
// lists the classes the bindings are generated for (the generator adds the
// ones the components need). --check writes nothing and fails when a file
// differs from what would be written, as the lint gate does.

const generatedFolder = path.join(import.meta.dirname, "..", "..", "src", "components", "generated");
const lockPath = [".winapp", "winmds.lock.json"];
const bindingsPath = [".winapp", "bindings"];

interface OutputFile {
  file: string;
  text: string;
}

function updatedPackageJson(packagePath: string, wanted: TypeName[]): OutputFile {
  return {
    file: packagePath,
    text: packageJsonWithBindings(readFileSync(packagePath, "utf8"), wanted)
  };
}

function generatedFiles(project: string): OutputFile[] {
  const model = buildModel(path.join(project, ...lockPath));
  const output: OutputFile[] = [
    {
      file: path.join(generatedFolder, "bindings.ts"),
      text: emitBindings(model)
    },
    {
      file: path.join(generatedFolder, "index.ts"),
      text: emitIndex(model)
    },
    {
      file: path.join(generatedFolder, "metadata.ts"),
      text: emitMetadata(model)
    },
    {
      file: path.join(generatedFolder, "required-bindings.json"),
      text: emitRequiredBindings(model)
    }
  ];
  for (const level of model.classes) {
    const name = level.type.name;
    output.push({
      file: path.join(generatedFolder, "controls", `${name}.svelte.ts`),
      text: emitLevel(level)
    });

    if (level.isComponent) {
      output.push(
        {
          file: path.join(generatedFolder, `${name}.svelte`),
          text: emitComponent(level)
        },
        {
          file: path.join(generatedFolder, `${name}.d.svelte.ts`),
          text: emitDeclaration(level)
        }
      );
    }
  }

  output.push(updatedPackageJson(path.join(project, "package.json"), requestedClasses(model)));
  reportMissingBindings(project, bindingClasses(model));
  return output;
}

// After `pnpm bindings`, a class the components import that the bindings do
// not hold would fail only when the window loads: say so now.
function reportMissingBindings(project: string, wanted: TypeName[]): void {
  const folder = path.join(project, ...bindingsPath);
  if (!existsSync(folder)) {
    return;
  }

  const missing = wanted.filter(type => {
    const module = bindingModule(type).replace("#winui", folder);
    return !existsSync(`${module}.js`);
  });
  if (missing.length > 0) {
    process.stderr.write(`bindings missing for ${missing.map(type => type.name).join(", ")}; run pnpm bindings\n`);
  }
}

function listFiles(folder: string): string[] {
  if (!existsSync(folder)) {
    return [];
  }

  return readdirSync(folder, {
    recursive: true,
    withFileTypes: true
  })
    .filter(entry => entry.isFile())
    .map(entry => path.join(entry.parentPath, entry.name));
}

// The generated files import the bindings as #winui/..., which the package's
// package.json maps into its own .winapp; Node's imports map cannot point out
// of the package, so .winapp here is a junction to the project's. The same
// goes for node_modules, where svelte and dynwinrt are installed. (Until the
// package is a workspace member with dependencies of its own.)
function linkProjectFolders(project: string): void {
  for (const name of [".winapp", "node_modules"]) {
    const link = path.join(import.meta.dirname, "..", "..", name);
    const target = path.join(project, name);
    const canLink = !existsSync(link) && existsSync(target);
    if (canLink) {
      symlinkSync(target, link, "junction");
    }
  }
}

function normalized(text: string): string {
  return text.replaceAll("\r\n", "\n");
}

const { values } = parseArgs({
  options: {
    project: {
      type: "string"
    },
    check: {
      type: "boolean"
    }
  }
});
if (values.project === undefined) {
  throw new Error("--project names the folder winapp restore was run in");
}

linkProjectFolders(path.resolve(values.project));
const output = generatedFiles(path.resolve(values.project));
const wanted = new Set(output.map(entry => entry.file));
const stale = listFiles(generatedFolder).filter(file => !wanted.has(file));
const differing = output.filter(entry => {
  const isPresent = existsSync(entry.file);
  return !isPresent || normalized(readFileSync(entry.file, "utf8")) !== normalized(entry.text);
});
if (values.check === true) {
  const problems = [...differing.map(entry => entry.file), ...stale];
  if (problems.length > 0) {
    process.stderr.write(`the generated components are stale; run pnpm gen:components\n${problems.join("\n")}\n`);
    process.exitCode = 1;
  }
} else {
  for (const entry of differing) {
    mkdirSync(path.dirname(entry.file), { recursive: true });
    writeFileSync(entry.file, entry.text);
  }

  for (const file of stale) {
    rmSync(file);
  }

  process.stdout.write(`${output.length} files, ${differing.length} written, ${stale.length} removed\n`);
}
