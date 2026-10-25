import type { SourcePosition } from "./types.ts";

// Svelte's css.map (a version 3 source map) read just far enough to turn a
// position in the CSS it emitted into the position in the .svelte file, so an
// error names the line the author wrote.

export interface SourceMapV3 {
  mappings: string;
}

interface Segment {
  generatedColumn: number;
  sourceLine: number;
  sourceColumn: number;
}

const base64Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const continuationBit = 32;
const valueMask = 31;
const bitsPerDigit = 5;

// The numbers of one segment (1, 4 or 5 of them), each a variable-length
// quantity whose lowest bit is its sign.
function decodeSegment(text: string): number[] {
  const numbers: number[] = [];
  let value = 0;
  let shift = 0;
  for (const character of text) {
    const digit = base64Digits.indexOf(character);
    value += (digit & valueMask) * 2 ** shift;
    const isLastDigit = (digit & continuationBit) === 0;
    if (isLastDigit) {
      const isNegative = value % 2 === 1;
      const magnitude = Math.floor(value / 2);
      numbers.push(isNegative ? -magnitude : magnitude);
      value = 0;
      shift = 0;
    } else {
      shift += bitsPerDigit;
    }
  }

  return numbers;
}

function decodeLines(mappings: string): Segment[][] {
  let sourceLine = 0;
  let sourceColumn = 0;
  return mappings.split(";").map(line => {
    let generatedColumn = 0;
    const segments: Segment[] = [];
    for (const text of line.split(",")) {
      if (text === "") {
        continue;
      }

      const [columnDelta = 0, , lineDelta, columnDeltaInSource] = decodeSegment(text);
      generatedColumn += columnDelta;
      const hasSource = lineDelta !== undefined && columnDeltaInSource !== undefined;
      if (hasSource) {
        sourceLine += lineDelta;
        sourceColumn += columnDeltaInSource;
        segments.push({
          generatedColumn,
          sourceLine,
          sourceColumn
        });
      }
    }

    return segments;
  });
}

export type PositionMapper = (generated: SourcePosition) => SourcePosition;

export function createPositionMapper(map: SourceMapV3 | undefined): PositionMapper {
  if (map === undefined) {
    return generated => generated;
  }

  const lines = decodeLines(map.mappings);
  return generated => {
    const segments = lines[generated.line - 1] ?? [];
    const column = generated.column - 1;
    const segment = segments.findLast(candidate => candidate.generatedColumn <= column);
    if (segment === undefined) {
      return generated;
    }

    return {
      line: segment.sourceLine + 1,
      column: segment.sourceColumn + (column - segment.generatedColumn) + 1
    };
  };
}
