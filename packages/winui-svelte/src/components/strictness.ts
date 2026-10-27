// Type-level checks that every prop of every component has a real type: a prop
// that resolved to any or unknown accepts anything, so a wrong value in markup
// passes unseen. generated/props-strictness.ts applies them to each component;
// `tsc` fails there naming the component and its loose props.

type IsAny<Type> = 0 extends 1 & Type ? true : false;

// Every value: what unknown is, spelled as its parts.
type AnyValue = object | string | number | bigint | boolean | symbol | null | undefined;

// any, or a type every value is assignable to (unknown).
type IsLoose<Type> = IsAny<Type> extends true ? true : [AnyValue] extends [Type] ? true : false;

// The props whose type is loose; never when there is none.
type LooseKeys<Props> = {
  [Key in keyof Props]-?: IsLoose<Props[Key]> extends true ? Key : never;
}[keyof Props];

// true, or what is wrong: the component's name and its loose props.
export type StrictProps<Name extends string, Props> = [LooseKeys<Props>] extends [never]
  ? true
  : {
    component: Name;
    looseProps: LooseKeys<Props>;
  };

// Fails to compile unless the check is true.
export type Expect<Check extends true> = Check;
