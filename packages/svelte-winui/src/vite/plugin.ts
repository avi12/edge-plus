import { winuiRenderer } from "./compile.ts";
import type { Options } from "@sveltejs/vite-plugin-svelte";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import type { CompileOptions } from "svelte/compiler";
import type { Plugin, UserConfig } from "vite";

// The one line in an app's Vite config: `plugins: [svelteWinui()]`. It is
// Svelte's own Vite plugin, told to compile components for the client against
// the WinUI renderer, plus the module environment the window's modules load
// in (see server.ts for the dev server that uses it).

export interface SvelteWinuiOptions {
  // The module a component imports its renderer from; default svelte-winui/host.
  renderer?: string;
}

export interface PluginOptions {
  svelte: Options;
  // What every component is compiled with, on top of its own settings.
  compile: CompileOptions;
}

// The environment the window's modules load in, which only the module runner
// of the window thread uses.
export const windowEnvironment = "winui";
const browserConditions = ["browser"];
// Svelte and esm-env pick their builds by condition, and svelte-winui is
// source, so Vite compiles them for the runner rather than leaving them to
// Node.
const compiledPackages = [/^svelte(?:\/|$)/u, "esm-env", "svelte-winui"];

// What the plugin passes on, without Vite: kept apart so it can be checked.
export function winuiPluginOptions(options: SvelteWinuiOptions = {}): PluginOptions {
  const compile: CompileOptions = {
    // The runner is Node, so the environment is a server one, but the window
    // mounts components: they compile for the client.
    generate: "client",
    experimental: {
      customRenderer: options.renderer ?? winuiRenderer
    }
  };
  return {
    compile,
    svelte: {
      configFile: false,
      // WinUI has no stylesheets, and a custom renderer takes no injected
      // CSS.
      emitCss: false,
      compilerOptions: {
        css: "external"
      },
      dynamicCompileOptions: () => compile
    }
  };
}

// The Vite settings the window's modules need.
export function windowConfig(): UserConfig {
  return {
    // Svelte's browser build is the one that mounts components.
    resolve: {
      conditions: browserConditions
    },
    // Nothing runs in a browser, so there is nothing to pre-bundle for one.
    optimizeDeps: {
      noDiscovery: true,
      include: []
    },
    environments: {
      [windowEnvironment]: {
        consumer: "server",
        resolve: {
          conditions: [...browserConditions, "development"],
          noExternal: compiledPackages
        },
        dev: {
          moduleRunnerTransform: true
        }
      }
    }
  };
}

export function svelteWinui(options: SvelteWinuiOptions = {}): Plugin[] {
  const environment: Plugin = {
    name: "svelte-winui:environment",
    config: windowConfig
  };
  return [environment, ...svelte(winuiPluginOptions(options).svelte)];
}
