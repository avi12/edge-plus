import { describeError } from "./diagnostics.ts";
import { svelteWinui, windowEnvironment } from "./plugin.ts";
import type { SvelteWinuiOptions } from "./plugin.ts";
import { logEvent } from "./runner.ts";
import path from "node:path";
import { pathToFileURL } from "node:url";
import type { MessagePort } from "node:worker_threads";
import type {
  CustomPayload,
  HotChannel,
  HotChannelClient,
  ResolvedConfig,
  ViteDevServer
} from "vite";
import { createServer, DevEnvironment } from "vite";
import type { FetchFunctionOptions, FetchResult } from "vite/module-runner";

// The development side of Svelte on WinUI: a Vite dev server, in-process and
// with no HTTP server, that compiles a window's modules for a module runner
// (runner.ts) on the window's thread and sends it hot updates, both over a
// MessagePort. Components compile for the client with HMR and render through
// the custom renderer; Svelte resolves to its browser build. The compile
// settings are the plugin's (plugin.ts), from the app's Vite config or, with
// none, the plugin's defaults.

export interface ModuleServerOptions extends SvelteWinuiOptions {
  // The folder of the window's sources, which Vite serves and watches.
  root: string;
  // The runner's end of this port is the window thread's.
  port: MessagePort;
  // Modules Node loads itself rather than Vite, besides the package's own
  // import site of the bindings: the app's import site of generated native
  // bindings.
  isNative?: (file: string) => boolean;
  // The app's Vite config, whose plugins should include svelteWinui(); without
  // one, the plugin's defaults serve.
  configFile?: string;
}

// The package's import sites of the bindings, which Node must load itself.
// Without an extension: from source they are .ts, from the published package
// .js.
const packageBindingsSites = [
  path.join(import.meta.dirname, "..", "winui"),
  path.join(import.meta.dirname, "..", "components", "generated", "bindings")
];
const queryOrHash = /[?#].*$/u;
const scriptExtension = /\.[cm]?[jt]s$/u;

function isSameFile(file: string, other: string): boolean {
  return path.relative(file, other) === "";
}

function isPackageBindingsSite(file: string): boolean {
  const withoutExtension = file.replace(queryOrHash, "").replace(scriptExtension, "");
  return packageBindingsSites.some(site => isSameFile(withoutExtension, site));
}

// What the runner sends: its module fetches and its own events.
function isCustomPayload(value: unknown): value is CustomPayload {
  return typeof value === "object" && value !== null && "type" in value && value.type === "custom";
}

type Listener = (data: unknown, client: HotChannelClient) => void;

// Vite's hot channel over a MessagePort: what the runner sends arrives as
// custom events (module fetches among them), and the server's updates go back.
function portChannel(port: MessagePort): HotChannel {
  const listeners = new Map<string, Set<Listener>>();
  const client: HotChannelClient = {
    send: payload => port.postMessage(payload)
  };
  return {
    // Only this process can reach the port.
    skipFsCheck: true,
    send: payload => port.postMessage(payload),
    on(event: string, listener: Listener) {
      const forEvent = listeners.get(event) ?? new Set();
      forEvent.add(listener);
      listeners.set(event, forEvent);
    },
    off(event, listener) {
      for (const registered of listeners.get(event) ?? []) {
        if (registered === listener) {
          listeners.get(event)?.delete(registered);
        }
      }
    },
    listen() {
      port.on("message", (message: unknown) => {
        if (!isCustomPayload(message)) {
          return;
        }

        for (const listener of listeners.get(message.event) ?? []) {
          listener(message.data, client);
        }
      });
    },
    close() {
      port.close();
    }
  };
}

class WindowEnvironment extends DevEnvironment {
  private readonly isNative: (file: string) => boolean;

  constructor(name: string, config: ResolvedConfig, port: MessagePort, isNative: (file: string) => boolean) {
    super(name, config, {
      hot: true,
      transport: portChannel(port)
    });
    this.isNative = isNative;
    this.hot.on(logEvent, (line: unknown) => {
      process.stderr.write(`[vite] ${String(line)}\n`);
    });
  }

  override async fetchModule(id: string, importer?: string, options?: FetchFunctionOptions): Promise<FetchResult> {
    const resolved = await this.pluginContainer.resolveId(id, importer);
    const file = resolved?.id.replace(queryOrHash, "");
    const isNativeFile = file !== undefined && this.isNative(file);
    if (isNativeFile) {
      return {
        externalize: pathToFileURL(file).href,
        type: "module"
      };
    }

    // The runner only learns that a fetch failed; the developer needs the file
    // and line, here on the host's stderr.
    try {
      return await super.fetchModule(id, importer, options);
    } catch (error) {
      process.stderr.write(`[vite] ${describeError(error)}\n`);
      throw error;
    }
  }
}

export async function startModuleServer(options: ModuleServerOptions): Promise<ViteDevServer> {
  function isNative(file: string): boolean {
    return isPackageBindingsSite(file) || options.isNative?.(file) === true;
  }

  const hasConfigFile = options.configFile !== undefined;
  const server = await createServer({
    configFile: options.configFile ?? false,
    root: options.root,
    appType: "custom",
    clearScreen: false,
    logLevel: "warn",
    server: {
      middlewareMode: true,
      ws: false
    },
    plugins: hasConfigFile ? [] : svelteWinui(options),
    environments: {
      [windowEnvironment]: {
        dev: {
          createEnvironment: (name, config) => new WindowEnvironment(name, config, options.port, isNative)
        }
      }
    }
  });
  return server;
}
