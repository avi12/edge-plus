import { readFileSync } from "node:fs";

// A reader for the ECMA-335 metadata inside a .winmd, enough for the
// generator: types, their interfaces, properties, events, methods and custom
// attributes. dynwinrt-codegen parses the same files, but it is a native
// program that only writes bindings (it has no way to dump what it read), so
// the generator reads the metadata itself.

const dosHeaderPeOffset = 0x3c;
const coffHeaderSize = 20;
const sectionHeaderSize = 40;
const optionalHeaderMagicPe32Plus = 0x20b;
const dataDirectoriesOffsetPe32 = 96;
const dataDirectoriesOffsetPe32Plus = 112;
const dataDirectorySize = 8;
const cliHeaderDirectoryIndex = 14;
const metadataRootSignature = 0x424a5342;
const tableCount = 64;
const wideStringsFlag = 0x01;
const wideGuidsFlag = 0x02;
const wideBlobsFlag = 0x04;
const smallIndexLimit = 0x10000;
const nullStringMarker = 0xff;

const TableId = {
  Module: 0x00,
  TypeRef: 0x01,
  TypeDef: 0x02,
  Field: 0x04,
  MethodDef: 0x06,
  Param: 0x08,
  InterfaceImpl: 0x09,
  MemberRef: 0x0a,
  Constant: 0x0b,
  CustomAttribute: 0x0c,
  Event: 0x14,
  EventMap: 0x12,
  Property: 0x17,
  PropertyMap: 0x15,
  MethodSemantics: 0x18,
  TypeSpec: 0x1b,
  GenericParam: 0x2a
} as const;

export const TypeAttribute = {
  Interface: 0x20,
  Abstract: 0x80,
  Sealed: 0x100
} as const;

const MethodAttribute = {
  Static: 0x10
} as const;

const MethodSemantic = {
  Setter: 0x01,
  Getter: 0x02,
  AddOn: 0x08,
  RemoveOn: 0x10
} as const;

type ColumnKind = string;

// A column is a fixed width ("2", "4"), a heap index ("str", "guid", "blob"), a
// plain index into another table ("t:<id>") or a coded index ("c:<name>").
const tableColumns: Record<number, ColumnKind[]> = {
  0x00: ["2", "str", "guid", "guid", "guid"],
  0x01: ["c:ResolutionScope", "str", "str"],
  0x02: ["4", "str", "str", "c:TypeDefOrRef", "t:4", "t:6"],
  0x03: ["t:4"],
  0x04: ["2", "str", "blob"],
  0x05: ["t:6"],
  0x06: ["4", "2", "2", "str", "blob", "t:8"],
  0x07: ["t:8"],
  0x08: ["2", "2", "str"],
  0x09: ["t:2", "c:TypeDefOrRef"],
  0x0a: ["c:MemberRefParent", "str", "blob"],
  0x0b: ["2", "c:HasConstant", "blob"],
  0x0c: ["c:HasCustomAttribute", "c:CustomAttributeType", "blob"],
  0x0d: ["c:HasFieldMarshal", "blob"],
  0x0e: ["2", "c:HasDeclSecurity", "blob"],
  0x0f: ["2", "4", "t:2"],
  0x10: ["4", "t:4"],
  0x11: ["blob"],
  0x12: ["t:2", "t:20"],
  0x13: ["t:20"],
  0x14: ["2", "str", "c:TypeDefOrRef"],
  0x15: ["t:2", "t:23"],
  0x16: ["t:23"],
  0x17: ["2", "str", "blob"],
  0x18: ["2", "t:6", "c:HasSemantics"],
  0x19: ["t:2", "c:MethodDefOrRef", "c:MethodDefOrRef"],
  0x1a: ["str"],
  0x1b: ["blob"],
  0x1c: ["2", "c:MemberForwarded", "str", "t:26"],
  0x1d: ["4", "t:4"],
  0x1e: ["4", "4"],
  0x1f: ["4"],
  0x20: ["4", "2", "2", "2", "2", "4", "blob", "str", "str"],
  0x21: ["4"],
  0x22: ["4", "4", "4"],
  0x23: ["2", "2", "2", "2", "4", "blob", "str", "str", "blob"],
  0x24: ["4", "t:35"],
  0x25: ["4", "4", "4", "t:35"],
  0x26: ["4", "str", "blob"],
  0x27: ["4", "4", "str", "str", "c:Implementation"],
  0x28: ["4", "4", "str", "c:Implementation"],
  0x29: ["t:2", "t:2"],
  0x2a: ["2", "2", "c:TypeOrMethodDef", "str"],
  0x2b: ["c:MethodDefOrRef", "blob"],
  0x2c: ["t:42", "c:TypeDefOrRef"]
};

const codedIndexTables: Record<string, Array<number | null>> = {
  TypeDefOrRef: [0x02, 0x01, 0x1b],
  HasConstant: [0x04, 0x08, 0x17],
  HasCustomAttribute: [
    0x06, 0x04, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x00, 0x0e, 0x17, 0x14, 0x11, 0x1a, 0x1b, 0x20, 0x23, 0x26, 0x27, 0x28,
    0x2a, 0x2c, 0x2b
  ],
  HasFieldMarshal: [0x04, 0x08],
  HasDeclSecurity: [0x02, 0x06, 0x20],
  MemberRefParent: [0x02, 0x01, 0x1a, 0x06, 0x1b],
  HasSemantics: [0x14, 0x17],
  MethodDefOrRef: [0x06, 0x0a],
  MemberForwarded: [0x04, 0x06],
  Implementation: [0x26, 0x23, 0x27],
  CustomAttributeType: [null, null, 0x06, 0x0a, null],
  ResolutionScope: [0x00, 0x1a, 0x23, 0x01],
  TypeOrMethodDef: [0x02, 0x06]
};

export interface TypeName {
  namespace: string;
  name: string;
}

// A type as a signature writes it.
export type TypeSignature =
  | {
    kind: "primitive";
    name: string;
  }
  | {
    kind: "named";
    type: TypeName;
    isValueType: boolean;
  }
  | {
    kind: "generic";
    type: TypeName;
    args: TypeSignature[];
  }
  | {
    kind: "array";
    element: TypeSignature;
  }
  | {
    kind: "parameter";
    index: number;
  };

export interface PropertyInfo {
  name: string;
  type: TypeSignature;
  hasGetter: boolean;
  hasSetter: boolean;
}

export interface EventInfo {
  name: string;
}

export interface MethodInfo {
  name: string;
  isStatic: boolean;
  parameters: Array<{
    name: string;
    type: TypeSignature;
  }>;
  returns: TypeSignature;
}

export interface AttributeInfo {
  type: TypeName;
  fixed: unknown[];
  named: Map<string, unknown>;
}

export interface InterfaceReference {
  type: TypeName | null;
  attributes: AttributeInfo[];
}

export interface TypeInfo {
  namespace: string;
  name: string;
  flags: number;
  base: TypeName | null;
  interfaces: InterfaceReference[];
  properties: PropertyInfo[];
  events: EventInfo[];
  methods: MethodInfo[];
  enumMembers: Array<{
    name: string;
    value: number;
  }>;
  attributes: AttributeInfo[];
}

export function fullName(type: TypeName): string {
  return `${type.namespace}.${type.name}`;
}

interface TableData {
  rowCount: number;
  rowSize: number;
  offset: number;
  columnSizes: number[];
}

class BlobReader {
  readonly bytes: Uint8Array;
  position: number;

  constructor(bytes: Uint8Array, position = 0) {
    this.bytes = bytes;
    this.position = position;
  }

  get isDone(): boolean {
    return this.position >= this.bytes.length;
  }

  byte(): number {
    const value = this.bytes[this.position] ?? 0;
    this.position++;
    return value;
  }

  compressed(): number {
    const first = this.byte();
    const isOneByte = (first & 0x80) === 0;
    if (isOneByte) {
      return first;
    }

    const isTwoBytes = (first & 0x40) === 0;
    if (isTwoBytes) {
      return ((first & 0x3f) << 8) | this.byte();
    }

    const second = this.byte();
    const third = this.byte();
    const fourth = this.byte();
    return ((first & 0x1f) << 24) | (second << 16) | (third << 8) | fourth;
  }

  integer(width: number): number {
    let value = 0;
    for (let index = 0; index < width; index++) {
      value += this.byte() * 2 ** (8 * index);
    }

    return value;
  }

  text(): string | null {
    const isNull = this.bytes[this.position] === nullStringMarker;
    if (isNull) {
      this.position++;
      return null;
    }

    const length = this.compressed();
    const text = Buffer.from(this.bytes.subarray(this.position, this.position + length)).toString("utf8");
    this.position += length;
    return text;
  }
}

export class WinMdFile {
  readonly types: TypeInfo[] = [];
  private readonly buffer: Buffer;
  private readonly tables = new Map<number, TableData>();
  private readonly strings: Buffer;
  private readonly blobs: Buffer;
  private readonly wideStrings: boolean;
  private readonly wideBlobs: boolean;
  private readonly wideGuids: boolean;
  private readonly rowCounts = new Map<number, number>();
  private metadataOffset = 0;

  constructor(filePath: string) {
    this.buffer = readFileSync(filePath);
    const root = this.locateMetadata();
    const streams = this.readStreams(root);
    const tables = streams.get("#~");
    const strings = streams.get("#Strings");
    const blobs = streams.get("#Blob");
    const hasAllStreams = tables !== undefined && strings !== undefined && blobs !== undefined;
    if (!hasAllStreams) {
      throw new Error(`${filePath} is missing a metadata stream`);
    }

    this.strings = this.buffer.subarray(strings.offset, strings.offset + strings.size);
    this.blobs = this.buffer.subarray(blobs.offset, blobs.offset + blobs.size);
    const heapSizes = this.buffer.readUInt8(tables.offset + 6);
    this.wideStrings = (heapSizes & wideStringsFlag) !== 0;
    this.wideGuids = (heapSizes & wideGuidsFlag) !== 0;
    this.wideBlobs = (heapSizes & wideBlobsFlag) !== 0;
    this.layOutTables(tables.offset);
    this.readTypes();
  }

  private locateMetadata(): number {
    const peOffset = this.buffer.readUInt32LE(dosHeaderPeOffset);
    const coffOffset = peOffset + 4;
    const sectionCount = this.buffer.readUInt16LE(coffOffset + 2);
    const optionalSize = this.buffer.readUInt16LE(coffOffset + 16);
    const optionalOffset = coffOffset + coffHeaderSize;
    const isPe32Plus = this.buffer.readUInt16LE(optionalOffset) === optionalHeaderMagicPe32Plus;
    const directoriesOffset = optionalOffset + (isPe32Plus ? dataDirectoriesOffsetPe32Plus : dataDirectoriesOffsetPe32);
    const cliRva = this.buffer.readUInt32LE(directoriesOffset + cliHeaderDirectoryIndex * dataDirectorySize);
    const sectionsOffset = optionalOffset + optionalSize;
    const sections: Array<{
      rva: number;
      size: number;
      raw: number;
    }> = [];
    for (let index = 0; index < sectionCount; index++) {
      const headerOffset = sectionsOffset + index * sectionHeaderSize;
      sections.push({
        rva: this.buffer.readUInt32LE(headerOffset + 12),
        size: Math.max(this.buffer.readUInt32LE(headerOffset + 8), this.buffer.readUInt32LE(headerOffset + 16)),
        raw: this.buffer.readUInt32LE(headerOffset + 20)
      });
    }

    function toOffset(rva: number): number {
      const section = sections.find(candidate => rva >= candidate.rva && rva < candidate.rva + candidate.size);
      if (section === undefined) {
        throw new Error(`RVA ${rva} is in no section`);
      }

      return section.raw + (rva - section.rva);
    }
    const cliOffset = toOffset(cliRva);
    this.metadataOffset = toOffset(this.buffer.readUInt32LE(cliOffset + 8));
    return this.metadataOffset;
  }

  private readStreams(root: number): Map<string, {
    offset: number;
    size: number;
  }> {
    if (this.buffer.readUInt32LE(root) !== metadataRootSignature) {
      throw new Error("not a metadata root");
    }

    const versionLength = this.buffer.readUInt32LE(root + 12);
    const streamCount = this.buffer.readUInt16LE(root + 16 + versionLength + 2);
    let cursor = root + 16 + versionLength + 4;
    const streams = new Map<string, {
      offset: number;
      size: number;
    }>();
    for (let index = 0; index < streamCount; index++) {
      const offset = this.buffer.readUInt32LE(cursor);
      const size = this.buffer.readUInt32LE(cursor + 4);
      let end = cursor + 8;
      while (this.buffer[end] !== 0) {
        end++;
      }

      const name = this.buffer.toString("ascii", cursor + 8, end);
      streams.set(name, {
        offset: root + offset,
        size
      });
      const nameBytes = end - (cursor + 8) + 1;
      cursor += 8 + Math.ceil(nameBytes / 4) * 4;
    }

    return streams;
  }

  private layOutTables(start: number): void {
    const validLow = this.buffer.readUInt32LE(start + 8);
    const validHigh = this.buffer.readUInt32LE(start + 12);
    let cursor = start + 24;
    for (let id = 0; id < tableCount; id++) {
      const word = id < 32 ? validLow : validHigh;
      const isPresent = ((word >>> id % 32) & 1) === 1;
      if (isPresent) {
        this.rowCounts.set(id, this.buffer.readUInt32LE(cursor));
        cursor += 4;
      }
    }

    for (let id = 0; id < tableCount; id++) {
      const rowCount = this.rowCounts.get(id);
      const columns = tableColumns[id];
      const isUnsupported = rowCount !== undefined && columns === undefined;
      if (isUnsupported) {
        throw new Error(`metadata table ${id} is not supported`);
      }

      const isSupported = rowCount !== undefined && columns !== undefined;
      if (isSupported) {
        const columnSizes = columns.map(column => this.columnSize(column));
        const rowSize = columnSizes.reduce((total, size) => total + size, 0);
        this.tables.set(id, {
          rowCount,
          rowSize,
          offset: cursor,
          columnSizes
        });
        cursor += rowCount * rowSize;
      }
    }
  }

  private tableIndexSize(table: number): number {
    const rows = this.rowCounts.get(table) ?? 0;
    return rows < smallIndexLimit ? 2 : 4;
  }

  private codedIndexSize(name: string): number {
    const targets = codedIndexTables[name] ?? [];
    const tagBits = Math.ceil(Math.log2(targets.length));
    const largest = Math.max(...targets.map(target => target === null ? 0 : this.rowCounts.get(target) ?? 0));
    return largest < smallIndexLimit >> tagBits ? 2 : 4;
  }

  private columnSize(column: ColumnKind): number {
    const heapIsWide = new Map([
      ["str", this.wideStrings],
      ["guid", this.wideGuids],
      ["blob", this.wideBlobs]
    ]).get(column);
    if (heapIsWide !== undefined) {
      return heapIsWide ? 4 : 2;
    }

    if (column.startsWith("t:")) {
      return this.tableIndexSize(Number(column.slice(2)));
    }

    if (column.startsWith("c:")) {
      return this.codedIndexSize(column.slice(2));
    }

    return Number(column);
  }

  rowCount(table: number): number {
    return this.rowCounts.get(table) ?? 0;
  }

  // One cell of a 1-based row.
  cell(table: number, row: number, column: number): number {
    const data = this.tables.get(table);
    if (data === undefined) {
      return 0;
    }

    let cellOffset = data.offset + (row - 1) * data.rowSize;
    for (let index = 0; index < column; index++) {
      cellOffset += data.columnSizes[index] ?? 0;
    }

    const width = data.columnSizes[column] ?? 0;
    return width === 2 ? this.buffer.readUInt16LE(cellOffset) : this.buffer.readUInt32LE(cellOffset);
  }

  string(index: number): string {
    let end = index;
    while (this.strings[end] !== 0) {
      end++;
    }

    return this.strings.toString("utf8", index, end);
  }

  blob(index: number): BlobReader {
    const reader = new BlobReader(this.blobs, index);
    const length = reader.compressed();
    return new BlobReader(this.blobs.subarray(reader.position, reader.position + length));
  }

  // A coded index as its table and 1-based row.
  decode(coded: string, value: number): {
    table: number | null;
    row: number;
  } {
    const targets = codedIndexTables[coded] ?? [];
    const tagBits = Math.ceil(Math.log2(targets.length));
    const tag = value & ((1 << tagBits) - 1);
    return {
      table: targets[tag] ?? null,
      row: value >>> tagBits
    };
  }

  typeNameOf(coded: number): TypeName | null {
    const { table, row } = this.decode("TypeDefOrRef", coded);
    if (row === 0) {
      return null;
    }

    if (table === TableId.TypeDef) {
      return {
        namespace: this.string(this.cell(TableId.TypeDef, row, 2)),
        name: this.string(this.cell(TableId.TypeDef, row, 1))
      };
    }

    if (table === TableId.TypeRef) {
      return {
        namespace: this.string(this.cell(TableId.TypeRef, row, 2)),
        name: this.string(this.cell(TableId.TypeRef, row, 1))
      };
    }

    return null;
  }

  private readType(reader: BlobReader): TypeSignature {
    const element = reader.byte();
    const primitives: Record<number, string> = {
      0x01: "Void",
      0x02: "Boolean",
      0x03: "Char",
      0x04: "SByte",
      0x05: "Byte",
      0x06: "Int16",
      0x07: "UInt16",
      0x08: "Int32",
      0x09: "UInt32",
      0x0a: "Int64",
      0x0b: "UInt64",
      0x0c: "Single",
      0x0d: "Double",
      0x0e: "String",
      0x18: "IntPtr",
      0x19: "UIntPtr",
      0x1c: "Object"
    };
    const primitive = primitives[element];
    if (primitive !== undefined) {
      return {
        kind: "primitive",
        name: primitive
      };
    }

    const isValueType = element === 0x11;
    const isClass = element === 0x12;
    const isNamedType = isValueType || isClass;
    if (isNamedType) {
      const type = this.typeNameOf(reader.compressed());
      return {
        kind: "named",
        type: type ?? {
          namespace: "",
          name: "?"
        },
        isValueType
      };
    }

    const isGenericInstance = element === 0x15;
    if (isGenericInstance) {
      reader.byte();
      const type = this.typeNameOf(reader.compressed()) ?? {
        namespace: "",
        name: "?"
      };
      const count = reader.compressed();
      const args: TypeSignature[] = [];
      for (let index = 0; index < count; index++) {
        args.push(this.readType(reader));
      }

      return {
        kind: "generic",
        type,
        args
      };
    }

    const isArray = element === 0x1d;
    if (isArray) {
      return {
        kind: "array",
        element: this.readType(reader)
      };
    }

    const isParameter = element === 0x13 || element === 0x1e;
    if (isParameter) {
      return {
        kind: "parameter",
        index: reader.compressed()
      };
    }

    const isByReference = element === 0x10;
    const isModifier = element === 0x1f || element === 0x20;
    if (isByReference) {
      return this.readType(reader);
    }

    if (isModifier) {
      reader.compressed();
      return this.readType(reader);
    }

    throw new Error(`signature element 0x${element.toString(16)} is not supported`);
  }

  private readProperty(row: number): PropertyInfo {
    const reader = this.blob(this.cell(TableId.Property, row, 2));
    reader.byte();
    const parameterCount = reader.compressed();
    const type = this.readType(reader);
    for (let index = 0; index < parameterCount; index++) {
      this.readType(reader);
    }

    return {
      name: this.string(this.cell(TableId.Property, row, 1)),
      type,
      hasGetter: false,
      hasSetter: false
    };
  }

  private readMethod(row: number): MethodInfo {
    const reader = this.blob(this.cell(TableId.MethodDef, row, 4));
    const convention = reader.byte();
    const hasGenericParameters = (convention & 0x10) !== 0;
    if (hasGenericParameters) {
      reader.compressed();
    }

    const parameterCount = reader.compressed();
    const returns = this.readType(reader);
    const firstParam = this.cell(TableId.MethodDef, row, 5);
    const parameters: MethodInfo["parameters"] = [];
    for (let index = 0; index < parameterCount; index++) {
      const type = this.readType(reader);
      const paramRow = firstParam + index;
      const name = paramRow <= this.rowCount(TableId.Param) ? this.string(this.cell(TableId.Param, paramRow, 2)) : "";
      parameters.push({
        name,
        type
      });
    }

    return {
      name: this.string(this.cell(TableId.MethodDef, row, 3)),
      isStatic: (this.cell(TableId.MethodDef, row, 2) & MethodAttribute.Static) !== 0,
      parameters,
      returns
    };
  }

  private attributeValue(reader: BlobReader, type: TypeSignature): unknown {
    if (type.kind === "primitive") {
      const widths: Record<string, number> = {
        Boolean: 1,
        Byte: 1,
        SByte: 1,
        Int16: 2,
        UInt16: 2,
        Int32: 4,
        UInt32: 4,
        Int64: 8,
        UInt64: 8
      };
      const width = widths[type.name];
      if (type.name === "String") {
        return reader.text();
      }

      if (width !== undefined) {
        return reader.integer(width);
      }
    }

    const isSystemType = type.kind === "named" && type.type.name === "Type";
    if (isSystemType) {
      return reader.text();
    }

    if (type.kind === "named") {
      return reader.integer(4);
    }

    throw new Error("an attribute argument of this type is not supported");
  }

  // The attribute's class and the blob of its constructor's signature; null
  // for an attribute whose constructor is in this file or whose class is not
  // among the types read.
  private attributeConstructor(row: number): {
    type: AttributeInfo["type"];
    signatureIndex: number;
  } | null {
    const { table, row: constructorRow } = this.decode("CustomAttributeType", this.cell(TableId.CustomAttribute, row, 1));
    let typeCoded = 0;
    let signatureIndex = 0;
    if (table === TableId.MemberRef) {
      const parent = this.decode("MemberRefParent", this.cell(TableId.MemberRef, constructorRow, 0));
      typeCoded = parent.table === TableId.TypeRef ? (parent.row << 2) | 1 : (parent.row << 2);
      signatureIndex = this.cell(TableId.MemberRef, constructorRow, 2);
    } else if (table === TableId.MethodDef) {
      return null;
    }

    const type = this.typeNameOf(typeCoded);
    if (type === null) {
      return null;
    }

    return {
      type,
      signatureIndex
    };
  }

  private constructorParameters(signatureIndex: number): TypeSignature[] {
    const signature = this.blob(signatureIndex);
    signature.byte();
    const parameterCount = signature.compressed();
    signature.compressed();
    const parameterTypes: TypeSignature[] = [];
    for (let index = 0; index < parameterCount; index++) {
      parameterTypes.push(this.readType(signature));
    }

    return parameterTypes;
  }

  // The type of a named argument, from the serialization tag before its name.
  private namedArgumentType(kind: number): TypeSignature {
    const isString = kind === 0x0e || kind === 0x50;
    const primitive = kind === 0x02 ? "Boolean" : "Int32";
    return {
      kind: "primitive",
      name: isString ? "String" : primitive
    };
  }

  private readNamedArguments(value: BlobReader, named: Map<string, unknown>): void {
    const namedCount = value.integer(2);
    for (let index = 0; index < namedCount; index++) {
      value.byte();
      const kind = value.byte();
      const isEnum = kind === 0x55;
      if (isEnum) {
        value.text();
      }

      const name = value.text() ?? "";
      named.set(name, this.attributeValue(value, this.namedArgumentType(kind)));
    }
  }

  private readAttribute(row: number): AttributeInfo | null {
    const constructed = this.attributeConstructor(row);
    if (constructed === null) {
      return null;
    }

    const parameterTypes = this.constructorParameters(constructed.signatureIndex);
    const value = this.blob(this.cell(TableId.CustomAttribute, row, 2));
    const prolog = value.integer(2);
    const hasProlog = prolog === 1;
    const fixed: unknown[] = [];
    const named = new Map<string, unknown>();
    if (hasProlog) {
      try {
        for (const parameterType of parameterTypes) {
          fixed.push(this.attributeValue(value, parameterType));
        }

        this.readNamedArguments(value, named);
      } catch {
        // An argument this reader does not decode: the attributes the generator
        // reads have only strings, types and integers.
      }
    }

    return {
      type: constructed.type,
      fixed,
      named
    };
  }

  private attributesOf(
    parentTable: number,
    parentRow: number,
    byParent: Map<string, AttributeInfo[]>
  ): AttributeInfo[] {
    return byParent.get(`${parentTable}:${parentRow}`) ?? [];
  }

  private groupAttributes(): Map<string, AttributeInfo[]> {
    const byParent = new Map<string, AttributeInfo[]>();
    const tagged = codedIndexTables["HasCustomAttribute"] ?? [];
    for (let row = 1; row <= this.rowCount(TableId.CustomAttribute); row++) {
      const parent = this.decode("HasCustomAttribute", this.cell(TableId.CustomAttribute, row, 0));
      const isOfInterest = parent.table === TableId.TypeDef || parent.table === TableId.InterfaceImpl;
      const isTagged = isOfInterest && tagged.includes(parent.table);
      if (isTagged) {
        const attribute = this.readAttribute(row);
        if (attribute !== null) {
          const key = `${parent.table}:${parent.row}`;
          const list = byParent.get(key) ?? [];
          list.push(attribute);
          byParent.set(key, list);
        }
      }
    }

    return byParent;
  }

  private rangeEnd(table: number, row: number, column: number, ownerTable: number): number {
    const nextRow = row + 1;
    const total = this.rowCount(ownerTable);
    return nextRow <= this.rowCount(table) ? this.cell(table, nextRow, column) : total + 1;
  }

  private readTypes(): void {
    const byParent = this.groupAttributes();
    const propertyRanges = new Map<number, [number, number]>();
    for (let row = 1; row <= this.rowCount(TableId.PropertyMap); row++) {
      propertyRanges.set(this.cell(TableId.PropertyMap, row, 0), [
        this.cell(TableId.PropertyMap, row, 1),
        this.rangeEnd(TableId.PropertyMap, row, 1, TableId.Property)
      ]);
    }

    const eventRanges = new Map<number, [number, number]>();
    for (let row = 1; row <= this.rowCount(TableId.EventMap); row++) {
      eventRanges.set(this.cell(TableId.EventMap, row, 0), [
        this.cell(TableId.EventMap, row, 1),
        this.rangeEnd(TableId.EventMap, row, 1, TableId.Event)
      ]);
    }

    const semantics = new Map<string, number>();
    for (let row = 1; row <= this.rowCount(TableId.MethodSemantics); row++) {
      const association = this.decode("HasSemantics", this.cell(TableId.MethodSemantics, row, 2));
      const key = `${association.table}:${association.row}`;
      semantics.set(key, (semantics.get(key) ?? 0) | this.cell(TableId.MethodSemantics, row, 0));
    }

    const interfaces = new Map<number, InterfaceReference[]>();
    for (let row = 1; row <= this.rowCount(TableId.InterfaceImpl); row++) {
      const owner = this.cell(TableId.InterfaceImpl, row, 0);
      const list = interfaces.get(owner) ?? [];
      list.push({
        type: this.typeNameOf(this.cell(TableId.InterfaceImpl, row, 1)),
        attributes: this.attributesOf(TableId.InterfaceImpl, row, byParent)
      });
      interfaces.set(owner, list);
    }

    for (let row = 1; row <= this.rowCount(TableId.TypeDef); row++) {
      this.types.push(this.readTypeDefinition(row, byParent, propertyRanges, eventRanges, semantics, interfaces));
    }
  }

  private readTypeDefinition(
    row: number,
    byParent: Map<string, AttributeInfo[]>,
    propertyRanges: Map<number, [number, number]>,
    eventRanges: Map<number, [number, number]>,
    semantics: Map<string, number>,
    interfaces: Map<number, InterfaceReference[]>
  ): TypeInfo {
    const properties: PropertyInfo[] = [];
    const [firstProperty, endProperty] = propertyRanges.get(row) ?? [1, 1];
    for (let property = firstProperty; property < endProperty; property++) {
      const info = this.readProperty(property);
      const flags = semantics.get(`${TableId.Property}:${property}`) ?? 0;
      info.hasGetter = (flags & MethodSemantic.Getter) !== 0;
      info.hasSetter = (flags & MethodSemantic.Setter) !== 0;
      properties.push(info);
    }

    const events: EventInfo[] = [];
    const [firstEvent, endEvent] = eventRanges.get(row) ?? [1, 1];
    for (let event = firstEvent; event < endEvent; event++) {
      events.push({ name: this.string(this.cell(TableId.Event, event, 1)) });
    }

    const methods: MethodInfo[] = [];
    const firstMethod = this.cell(TableId.TypeDef, row, 5);
    const endMethod = this.rangeEnd(TableId.TypeDef, row, 5, TableId.MethodDef);
    for (let method = firstMethod; method < endMethod; method++) {
      methods.push(this.readMethod(method));
    }

    const flags = this.cell(TableId.TypeDef, row, 0);
    const base = this.typeNameOf(this.cell(TableId.TypeDef, row, 3));
    const isEnum = base?.name === "Enum";
    const enumMembers: TypeInfo["enumMembers"] = [];
    if (isEnum) {
      const firstField = this.cell(TableId.TypeDef, row, 4);
      const endField = this.rangeEnd(TableId.TypeDef, row, 4, TableId.Field);
      for (let field = firstField; field < endField; field++) {
        const fieldFlags = this.cell(TableId.Field, field, 0);
        const isLiteral = (fieldFlags & 0x40) !== 0;
        if (isLiteral) {
          enumMembers.push({
            name: this.string(this.cell(TableId.Field, field, 1)),
            value: this.constantOf(field)
          });
        }
      }
    }

    return {
      namespace: this.string(this.cell(TableId.TypeDef, row, 2)),
      name: this.string(this.cell(TableId.TypeDef, row, 1)),
      flags,
      base,
      interfaces: interfaces.get(row) ?? [],
      properties,
      events,
      methods,
      enumMembers,
      attributes: this.attributesOf(TableId.TypeDef, row, byParent)
    };
  }

  private fieldConstants: Map<number, number> | null = null;

  private constantOf(field: number): number {
    if (this.fieldConstants === null) {
      this.fieldConstants = new Map();
      for (let row = 1; row <= this.rowCount(TableId.Constant); row++) {
        const parent = this.decode("HasConstant", this.cell(TableId.Constant, row, 1));
        if (parent.table === TableId.Field) {
          const bytes = Buffer.from(this.blob(this.cell(TableId.Constant, row, 2)).bytes);
          const is64Bit = bytes.length === 8;
          this.fieldConstants.set(parent.row, is64Bit ? Number(bytes.readBigInt64LE(0)) : bytes.readInt32LE(0));
        }
      }
    }

    return this.fieldConstants.get(field) ?? 0;
  }
}

// Every type of a set of .winmd files, found by namespace and name.
export class TypeIndex {
  private readonly byName = new Map<string, TypeInfo>();

  add(file: WinMdFile): void {
    for (const type of file.types) {
      this.byName.set(`${type.namespace}.${type.name}`, type);
    }
  }

  find(type: TypeName): TypeInfo | undefined {
    return this.byName.get(fullName(type));
  }
}
