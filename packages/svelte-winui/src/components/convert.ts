import { read } from "../renderer/host/reflect.ts";
import {
  Application,
  BitmapImage,
  Brush,
  FontFamily,
  GridUnitType,
  IReference_Boolean,
  PropertyValue,
  SolidColorBrush,
  Uri
} from "./generated/bindings.ts";
import type { ImageSource } from "./generated/bindings.ts";
import type {
  AttachedInput,
  ColorInput,
  ColorValue,
  CornerRadiusInput,
  CornerRadiusValue,
  GridLengthParts,
  ThicknessInput,
  ThicknessValue
} from "./values.ts";
import {
  attachedList,
  cornerSides,
  gridLengthParts,
  GridUnit,
  parseColor,
  thicknessSides
} from "./values.ts";

// A prop's value as the bindings take it: the generated components call these
// where a prop is not already the bindings' own type.

export type { AttachedInput, ColorInput, CornerRadiusInput, ObjectInput, ThicknessInput } from "./values.ts";

// What every component takes besides its control's properties; the element
// itself gets them (the CSS runtime reads them).
export interface ElementProps {
  class?: string;
  style?: string;
}

// A member of an enum by name or value. A flag enum also takes a combination
// of its members' values.
export type EnumInput<Table extends Record<string, number>> = keyof Table | number;

export interface GridLengthValue {
  value: number;
  gridUnitType: GridUnitType;
}

export type GridLengthInput = number | string | GridLengthValue;
export type BrushInput = string | Brush;
export type ImageSourceInput = string | ImageSource;
export type FontFamilyInput = string | FontFamily;
export type UriInput = string | Uri;

const gridUnits = new Map<GridUnit, GridUnitType>([
  [GridUnit.Auto, GridUnitType.Auto],
  [GridUnit.Pixel, GridUnitType.Pixel],
  [GridUnit.Star, GridUnitType.Star]
]);
const maxInt32 = 2 ** 31 - 1;

function isFlagMask(members: readonly number[]): boolean {
  const flags = members.filter(member => member !== 0);
  const isEachABit = flags.every(member => (member & (member - 1)) === 0);
  return flags.length > 1 && isEachABit;
}

// A number is a member of the table, or a combination of the bits of a flag
// enum's members.
function isMember<Table extends Record<string, number>>(table: Table, value: unknown): value is Table[keyof Table] {
  const members: number[] = Object.values(table);
  const mask = members.reduce((all, member) => all | member, 0);
  const isCombination = typeof value === "number" && isFlagMask(members) && (value & ~mask) === 0;
  return members.some(member => member === value) || isCombination;
}

export function enumValue<Table extends Record<string, number>>(
  table: Table,
  input: EnumInput<Table>
): Table[keyof Table] {
  const value: unknown = typeof input === "string" ? read(table, input) : input;
  if (isMember(table, value)) {
    return value;
  }

  throw new RangeError(`${String(input)} is not one of ${Object.keys(table).join(", ")}`);
}

export function thicknessOf(input: ThicknessInput): ThicknessValue {
  const [left, top, right, bottom] = thicknessSides(input);
  return {
    left,
    top,
    right,
    bottom
  };
}

export function cornerRadiusOf(input: CornerRadiusInput): CornerRadiusValue {
  const [topLeft, topRight, bottomRight, bottomLeft] = cornerSides(input);
  return {
    topLeft,
    topRight,
    bottomRight,
    bottomLeft
  };
}

export function gridLengthOf(input: GridLengthInput): GridLengthValue {
  if (typeof input === "object") {
    return input;
  }

  const parts: GridLengthParts = gridLengthParts(input);
  const gridUnitType = gridUnits.get(parts.unit);
  if (gridUnitType === undefined) {
    throw new RangeError(`no grid unit for ${parts.unit}`);
  }

  return {
    value: parts.value,
    gridUnitType
  };
}

export function colorOf(input: ColorInput): ColorValue {
  if (typeof input === "object") {
    return input;
  }

  const color = parseColor(input);
  if (color === null) {
    throw new TypeError(`"${input}" is not a color (#rgb, #rrggbb or #aarrggbb)`);
  }

  return color;
}

function isBrush(value: object): value is Brush {
  return value instanceof Brush;
}

// A color makes a solid brush; any other string names a theme resource of the
// application, which holds the brush.
export function brushOf(input: BrushInput): Brush {
  if (typeof input === "object") {
    return input;
  }

  const color = parseColor(input);
  if (color !== null) {
    return new SolidColorBrush(color);
  }

  const resources = Application.current.resources;
  const key = PropertyValue.createString(input);
  if (!resources.hasKey(key)) {
    throw new TypeError(`"${input}" is neither a color nor a resource the application holds`);
  }

  const found = resources.lookup(key);
  const isBrushResource = typeof found === "object" && found !== null && isBrush(found);
  if (!isBrushResource) {
    throw new TypeError(`the resource "${input}" is not a brush`);
  }

  return found;
}

export type NullableBooleanInput = boolean | null;

// A nullable boolean property (ToggleButton.IsChecked) takes a boxed value.
export function nullableBooleanOf(input: NullableBooleanInput): IReference_Boolean | null {
  return input === null ? null : IReference_Boolean.from(PropertyValue.createBoolean(input));
}

export function uriOf(input: UriInput): Uri {
  return typeof input === "string" ? new Uri(input) : input;
}

export function fontFamilyOf(input: FontFamilyInput): FontFamily {
  return typeof input === "string" ? new FontFamily(input) : input;
}

export function imageSourceOf(input: ImageSourceInput): ImageSource {
  if (typeof input !== "string") {
    return input;
  }

  const image = new BitmapImage();
  image.uriSource = new Uri(input);
  return image;
}

// A property of type object takes only a boxed value: a string, number or
// boolean is boxed, anything else is the WinRT object it already is.
export function objectOf(input: unknown): unknown {
  if (typeof input === "string") {
    return PropertyValue.createString(input);
  }

  if (typeof input === "boolean") {
    return PropertyValue.createBoolean(input);
  }

  if (typeof input === "number") {
    const isInteger = Number.isInteger(input) && Math.abs(input) <= maxInt32;
    return isInteger ? PropertyValue.createInt32(input) : PropertyValue.createDouble(input);
  }

  return input;
}

// Calls each Owner.set<Property>(control, value) an `attached` prop lists.
export function setAttached(control: object, input: AttachedInput): void {
  for (const [owner, property, value] of attachedList(input)) {
    const setter = read(owner, `set${property}`);
    if (typeof setter !== "function") {
      throw new TypeError(`attached={[Owner, "${property}", …]}: the owner has no set${property}`);
    }

    setter.apply(owner, [control, value]);
  }
}
