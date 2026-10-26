<script lang="ts">
  import { ListView, MoveDirection, Window } from "svelte-winui/components";
  import type { ListHandle } from "svelte-winui/components";
  import { exitApplication } from "svelte-winui/window";
  import type { ExampleHost } from "../harness/contract.ts";
  import { makeItems } from "./example-item.ts";
  import ExampleRow from "./example-row.svelte";
  import { liveRows, rowStats } from "./row-stats.ts";

  const defaultRows = 1000;
  const { host }: { host: ExampleHost } = $props();

  let rows = $state(makeItems(Number(host.options["rows"] ?? defaultRows)));
  let handle: ListHandle | undefined = $state();
  let reorders = 0;

  function report(kind: string): void {
    host.report({
      kind,
      rows: rows.length,
      created: rowStats.created,
      destroyed: rowStats.destroyed,
      live: liveRows(),
      reorders,
      first: rows.slice(0, 5).map(row => row.id)
    });
  }

  function reorder(order: string[]): void {
    reorders++;
    const byId = new Map(rows.map(row => [row.id, row]));
    rows = order.flatMap(id => byId.get(id) ?? []);
  }

  // What a test or a script can ask of the window.
  const commands = new Map<string, (command: Record<string, unknown>) => void>([
    ["stats", () => report("stats")],
    ["move", command => {
      const direction = command["direction"] === "up" ? MoveDirection.Up : MoveDirection.Down;
      handle?.move(String(command["key"]), direction);
      report("moved");
    }],
    ["scroll", command => {
      handle?.scrollToKey(String(command["key"]));
      report("scrolled");
    }],
    ["focus", command => {
      handle?.focus(String(command["key"]));
      report("focused");
    }],
    ["rename", command => {
      const row = rows.find(candidate => candidate.id === command["key"]);
      if (row !== undefined) {
        row.name = String(command["name"]);
      }

      report("renamed");
    }],
    ["remove", command => {
      rows = rows.filter(row => row.id !== command["key"]);
      report("removed");
    }],
    ["clear", () => {
      rows = [];
      report("cleared");
    }],
    ["close", () => exitApplication()]
  ]);
  host.onCommand(command => commands.get(String(command["cmd"]))?.(command));
</script>

<Window
  title="Lists example"
  width={420}
  height={640}
  placement={{ x: -8000, y: 0 }}
  activate={false}
  onClosed={exitApplication}
>
  <ListView
    items={rows}
    key={row => row.id}
    itemName={row => row.name}
    reorderable
    keyboardReorder
    selectionMode="None"
    onReorder={reorder}
    bind:handle
  >
    {#snippet item(row)}
      <ExampleRow {row} />
    {/snippet}
  </ListView>
</Window>
