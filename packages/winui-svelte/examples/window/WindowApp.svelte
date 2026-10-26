<script lang="ts">
  import type { Json } from "../../src/messaging/json.ts";
  import { DesktopAcrylicBackdrop, MicaBackdrop, MicaKind, projectAs } from "../../src/winui.ts";
  import type { ExampleCommand, ExampleHost } from "../harness/contract.ts";
  import { flush } from "winui-svelte";
  import { Window } from "winui-svelte/components";
  import type { WindowBounds, WindowHandle } from "winui-svelte/components";
  import { exitApplication } from "winui-svelte/window";
  import { Backdrop } from "winui-svelte/window";

  // The material named, or acrylic.
  function backdropOf(name: Json | undefined): Backdrop {
    return Object.values(Backdrop).find(candidate => candidate === name) ?? Backdrop.Acrylic;
  }

  const { host, icon }: {
    host: ExampleHost;
    icon: string;
  } = $props();

  let backdrop = $state(backdropOf(host.options["backdrop"]));
  let title = $state("Window example");
  let width = $state(520);
  let handle: WindowHandle | undefined = $state();
  let lastBounds: WindowBounds | null = null;
  let boundsReports = 0;
  let activations = 0;
  let closes = 0;

  // The kind of material the window holds now, read back from WinUI.
  function backdropName(): string {
    const held = handle?.window.systemBackdrop;
    const isUnset = held === undefined || held === null;
    if (isUnset) {
      return "none";
    }

    // WinUI hands back the base class; projecting to the wrong one throws.
    try {
      return projectAs(held, MicaBackdrop).kind === MicaKind.BaseAlt ? "micaAlt" : "mica";
    } catch {
      return projectAs(held, DesktopAcrylicBackdrop) instanceof DesktopAcrylicBackdrop ? "acrylic" : "unknown";
    }
  }

  // Reports what the window shows after the props the command changed have
  // reached it.
  function report(kind: string): void {
    flush();
    const appWindow = handle?.appWindow;
    host.report({
      kind,
      backdrop: backdropName(),
      title: handle?.window.title,
      scale: handle?.scale,
      extendsIntoTitleBar: handle?.window.extendsContentIntoTitleBar,
      width: appWindow?.size.width,
      height: appWindow?.size.height,
      x: appWindow?.position.x,
      y: appWindow?.position.y,
      minimumWidth: handle?.presenter.preferredMinimumWidth,
      isMaximizable: handle?.presenter.isMaximizable,
      boundsReports,
      lastBounds,
      activations,
      closes
    });
  }

  const commands = new Map<string, (command: ExampleCommand) => void>([
    ["info", () => report("info")],
    ["backdrop", command => {
      backdrop = backdropOf(command["value"]);
      report("backdrop");
    }],
    ["title", command => {
      title = String(command["value"]);
      report("title");
    }],
    ["width", command => {
      width = Number(command["value"]);
      report("width");
    }],
    ["move", command => {
      handle?.appWindow.move({
        x: Number(command["x"]),
        y: Number(command["y"])
      });
      report("moved");
    }],
    ["close", () => handle?.close()]
  ]);
  host.onCommand(command => commands.get(String(command["cmd"]))?.(command));
</script>

<Window
  activate={false}
  appUserModelId="winui-svelte.examples.window"
  {backdrop}
  height={360}
  {icon}
  isMaximizable={false}
  minWidth={400}
  onActiveChanged={isActive => {
    if (isActive) {
      activations++;
    }
  }}
  onBoundsChanged={bounds => {
    boundsReports++;
    lastBounds = bounds;
  }}
  onClosed={() => {
    closes++;
    exitApplication();
  }}
  placement={{
    x: -8000,
    y: 0
  }}
  {title}
  {width}
  bind:handle
>
  {#snippet titleBar()}
    <stack-panel height={40} orientation="Horizontal" padding="14,0,0,0" spacing={10}>
      <text-block text={title} vertical-alignment="Center" />
    </stack-panel>
  {/snippet}
  <stack-panel padding="24" spacing={8}>
    <text-block font-size={20} text={`Backdrop: ${backdrop}`} />
    <text-block text="The content runs into the title bar; the backdrop shows through." />
  </stack-panel>
</Window>
