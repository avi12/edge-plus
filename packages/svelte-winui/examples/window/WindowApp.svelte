<script lang="ts">
  import { flush } from "svelte-winui";
  import { Window } from "svelte-winui/components";
  import type { WindowBounds, WindowHandle } from "svelte-winui/components";
  import { exitApplication } from "svelte-winui/window";
  import { Backdrop } from "svelte-winui/window";
  import { DesktopAcrylicBackdrop, MicaBackdrop, MicaKind, projectAs } from "../../src/winui.ts";
  import type { ExampleHost } from "../harness/contract.ts";

  // The material named, or acrylic.
  function backdropOf(name: unknown): Backdrop {
    return Object.values(Backdrop).find(candidate => candidate === name) ?? Backdrop.Acrylic;
  }

  const { host, icon }: { host: ExampleHost; icon: string } = $props();

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
    if (held === undefined || held === null) {
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

  const commands = new Map<string, (command: Record<string, unknown>) => void>([
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
      handle?.appWindow.move({ x: Number(command["x"]), y: Number(command["y"]) });
      report("moved");
    }],
    ["close", () => handle?.close()]
  ]);
  host.onCommand(command => commands.get(String(command["cmd"]))?.(command));
</script>

<Window
  {title}
  {backdrop}
  {icon}
  {width}
  height={360}
  minWidth={400}
  isMaximizable={false}
  placement={{ x: -8000, y: 0 }}
  activate={false}
  appUserModelId="svelte-winui.examples.window"
  bind:handle
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
>
  {#snippet titleBar()}
    <stack-panel orientation="Horizontal" height={40} padding="14,0,0,0" spacing={10}>
      <text-block text={title} vertical-alignment="Center" />
    </stack-panel>
  {/snippet}
  <stack-panel padding="24" spacing={8}>
    <text-block text={`Backdrop: ${backdrop}`} font-size={20} />
    <text-block text="The content runs into the title bar; the backdrop shows through." />
  </stack-panel>
</Window>
