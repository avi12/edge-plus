import { gridLengthOf } from "../convert.ts";
import { ColumnDefinition, RowDefinition } from "../generated/bindings.ts";
import type { Grid } from "../generated/bindings.ts";
import type { Linker } from "../runtime.svelte.ts";

// Hand-written props of the generated Grid: WinUI's ColumnDefinitions and
// RowDefinitions are read-only collections, which the generator leaves out
// (tables.ts, extendedClasses). A string lists the lengths, as XAML's
// shorthand and the raw <grid columns="..."> element do.
//
//   <Grid columns="Auto,*,Auto" rows="Auto,Auto">

const listSeparator = ",";

export interface GridExtras {
  // "Auto", "*", "2*" or a number of pixels, separated by commas.
  columns?: string;
  rows?: string;
}

type Definition = ColumnDefinition | RowDefinition;

// The lengths replace the definitions the grid holds.
function define<Item extends Definition>(
  definitions: {
    clear(): void;
    append(item: Item): void;
  },
  make: (length: string) => Item,
  lengths: string
): void {
  definitions.clear();
  for (const length of lengths.split(listSeparator)) {
    definitions.append(make(length.trim()));
  }
}

function column(length: string): ColumnDefinition {
  const definition = new ColumnDefinition();
  definition.width = gridLengthOf(length);
  return definition;
}

function row(length: string): RowDefinition {
  const definition = new RowDefinition();
  definition.height = gridLengthOf(length);
  return definition;
}

export function linkGridExtras(control: Grid, props: GridExtras, link: Linker): void {
  link.property(props, "columns", lengths => {
    define(control.columnDefinitions, column, lengths);
  });
  link.property(props, "rows", lengths => {
    define(control.rowDefinitions, row, lengths);
  });
}
