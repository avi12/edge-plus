#!/usr/bin/env node
import { setup } from "./setup.ts";
import path from "node:path";
import { parseArgs } from "node:util";

// The `svelte-winui` command.

const usage = `svelte-winui setup [directory] [--dry-run] [--winapp <command>]

  setup   Prepares an app to use the controls: winapp.yaml, the classes the
          package needs in package.json's winapp.jsBindings, the
          @winapp/bindings dependency, and the bindings themselves, generated
          with winapp (restore, then node generate-bindings).

  directory   the app's folder (the current folder by default)
  --dry-run   write the files, do not run winapp
  --winapp    the winapp command, when it is not in node_modules/.bin or on PATH
`;

const { values, positionals } = parseArgs({
  allowPositionals: true,
  options: {
    "dry-run": {
      type: "boolean"
    },
    winapp: {
      type: "string"
    },
    help: {
      type: "boolean"
    }
  }
});
const [command, directory] = positionals;
const isSetup = command === "setup";
const isHelp = values.help === true;
const shouldSetUp = isSetup && !isHelp;
if (shouldSetUp) {
  setup({
    directory: path.resolve(directory ?? "."),
    ...values.winapp === undefined ? {} : { winapp: values.winapp },
    isDryRun: values["dry-run"] === true
  });
} else {
  process.stdout.write(usage);
  process.exitCode = isSetup || isHelp ? 0 : 1;
}
