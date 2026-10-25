import { logEvent } from "./vite-runner.ts";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import { pathToFileURL } from "node:url";
import type { MessagePort } from "node:worker_threads";
import type { CompileOptions } from "svelte/compiler";
import type {
  CustomPayload,
  HotChannel,
  HotChannelClient,
  PluginOption,
  ResolvedConfig,
  ViteDevServer
} from "vite";
import { createServer, DevEnvironment } from "vite";
import type { FetchFunctionOptions, FetchResult } from "vite/module-runner";

// The development side of Svelte on WinUI: a Vite dev server, in-process and
// with no HTTP server, that compiles a window's modules for a module runner
// (vite-runner.ts) on the window's thread and sends it hot updates, both over
// a MessagePort. Components compile for the client with HMR and render
// through the app's custom renderer; Svelte resolves to its browser build.

export interface ModuleServerOptions {
  // The folder of the window's sources, which Vite serves and watches.
  root: string;
  // The runner's end of this port is the window thread's.
  port: MessagePort;
  // The custom renderer a component imports, as an import specifier relative
  // to the component.
  customRenderer: (component: string) => string;
  // Modules Node loads itself rather than Vite, such as the one that imports
  // generated native bindings.
  isNative: (file: string) => boolean;
  // Vite plugins that run after the component is compiled, such as the one
  // that registers a component's <style> as WinUI Styles.
  afterSvelte?: PluginOption[];
  // Compile options for every component, such as the scoping class's hash.
  compilerOptions?: CompileOptions;
}

const environmentName = "winui";
const browserCondition = "browser";
// Svelte and esm-env pick their builds by condition, so Vite compiles them for
// the runner rather than leaving them to Node.
const compiledPackages = [/^svelte(?:\/|$)/u, "esm-env"];
const queryOrHash = /[?#].*$/u;

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

    return super.fetchModule(id, importer, options);
  }
}

export async function startModuleServer(options: ModuleServerOptions): Promise<ViteDevServer> {
  const server = await createServer({
    configFile: false,
    root: options.root,
    appType: "custom",
    clearScreen: false,
    logLevel: "warn",
    server: {
      middlewareMode: true,
      ws: false
    },
    // Nothing runs in a browser, so there is nothing to pre-bundle for one.
    optimizeDeps: {
      noDiscovery: true,
      include: []
    },
    plugins: [
      svelte({
        configFile: false,
        // WinUI has no stylesheets, and a custom renderer takes no injected
        // CSS.
        emitCss: false,
        compilerOptions: {
          ...options.compilerOptions,
          css: "external"
        },
        // The runner is Node, so the environment is a server one, but the
        // window mounts components: they compile for the client.
        dynamicCompileOptions: ({ filename }) => ({
          generate: "client",
          experimental: {
            customRenderer: options.customRenderer(filename)
          }
        })
      }),
      ...options.afterSvelte ?? []
    ],
    environments: {
      [environmentName]: {
        consumer: "server",
        resolve: {
          conditions: [browserCondition, "development"],
          noExternal: compiledPackages
        },
        dev: {
          moduleRunnerTransform: true,
          createEnvironment: (name, config) => new WindowEnvironment(name, config, options.port, options.isNative)
        }
      }
    }
  });
  return server;
}
