import type { TypeName } from "./winmd.ts";

// What the metadata cannot say, or says too loosely, in one place: the
// generator reads these tables and nothing else decides it.

// Where a component hierarchy starts. Everything below is a component or the
// abstract base one inherits from.
export const rootClasses: TypeName[] = [
  {
    namespace: "Microsoft.UI.Xaml",
    name: "UIElement"
  },
  {
    namespace: "Microsoft.UI.Xaml.Documents",
    name: "TextElement"
  }
];

// The winmd the components are read from; the rest only resolve the types
// their properties name.
export const componentWinmd = "Microsoft.UI.Xaml.winmd";
export const referenceWinmds = [
  "Microsoft.UI.Text.winmd",
  "Microsoft.UI.winmd",
  "Microsoft.Foundation.winmd",
  "Microsoft.Graphics.winmd",
  "Microsoft.Web.WebView2.Core.winmd",
  "Windows.Foundation.FoundationContract.winmd",
  "Windows.Foundation.UniversalApiContract.winmd"
];

// An interface a class implements for the code that derives from it, not for
// the code that uses it.
export const subclassInterfaceSuffixes = ["Protected", "Overrides", "Private", "Factory", "Statics"];

// Events a class declares but WinUI does not implement: subscribing returns
// E_NOTIMPL. By the full name of the class that declares them.
export const unimplementedEvents = new Map<string, string[]>([
  [
    "Microsoft.UI.Xaml.Documents.TextElement",
    ["AccessKeyDisplayDismissed", "AccessKeyDisplayRequested", "AccessKeyInvoked"]
  ]
]);

// A property a component does not take, and why.
export const skippedProperties = new Map<string, string>([
  ["Style", "style is the CSS string of the element; WinUI's Style object is set from the class"]
]);

// A component with props the metadata cannot give (Grid's columns and rows
// are read-only collections): src/components/extensions/<kebab-name>.ts
// exports `<Name>Extras` (the props) and `link<Name>Extras(control, props,
// link)`, which the generated level mixes in.
export const extendedClasses = new Set(["Grid"]);

// A property that has its own props `children`, `class`, `style` or
// `attached`, which every component takes.
export const reservedKeys = new Set(["children", "class", "style", "attached"]);

// A class WinUI activates but the generator leaves out, and why. Filled from
// the mount test: a control the test cannot construct on its own.
export const nonConstructible = new Map<string, string>([
  [
    "Microsoft.UI.Xaml.Controls.SwapChainPanel",
    "dynwinrt-codegen cannot emit it with the other controls: it reaches Microsoft.UI.Input.PointerEventArgs and "
    + "Windows.UI.Core.PointerEventArgs in one pass, and the second pass names the latter differently from the first"
  ],
  [
    "Microsoft.UI.Xaml.Controls.SwapChainBackgroundPanel",
    "as SwapChainPanel"
  ],
  [
    "Microsoft.UI.Xaml.Controls.Primitives.PivotHeaderItem",
    "constructing it fails with E_NOINTERFACE: it is activatable in the metadata but WinUI makes it only inside a Pivot"
  ],
  [
    "Microsoft.UI.Xaml.Controls.WebView2",
    "its events name Microsoft.Web.WebView2.Core types, which winapp does not give dynwinrt-codegen, so the "
    + "binding for the control cannot be emitted"
  ]
]);

// A property whose value changes by the user raises an event; the property
// is then $bindable(). The rule `<Property>Changed` finds the regular ones;
// these are the irregular, each listing the events that say the value
// changed. An event the class does not have is left out.
export const bindableEvents = new Map<string, string[]>([
  ["IsOn", ["Toggled"]],
  ["SelectedIndex", ["SelectionChanged"]],
  ["SelectedItem", ["SelectionChanged"]],
  ["SelectedValue", ["SelectionChanged"]],
  ["IsChecked", ["Checked", "Unchecked", "Indeterminate"]],
  ["IsDropDownOpen", ["DropDownOpened", "DropDownClosed"]],
  ["IsExpanded", ["Expanding", "Collapsed"]],
  ["IsOpen", ["Opened", "Closed"]],
  ["IsPaneOpen", ["PaneOpened", "PaneClosed"]]
]);

// A property the rule would make bindable but is not the control's to give
// back: the data context is pushed down by the owner, and its change event
// also fires when an ancestor's changes; IsEnabled is the effective value, so
// a disabled parent would write false into an enabled child for good.
export const neverBindable = new Set(["DataContext", "IsEnabled"]);

// Classes whose static Set<Name>(element, value) methods are attached
// properties, each as a prop named by the prefix and the property: gridRow,
// automationName, toolTip. A property that starts with the prefix already
// keeps its own name (AutomationId is automationId, not automationAutomationId).
export const attachedOwners: Array<{
  owner: TypeName;
  prefix: string;
}> = [
  {
    owner: {
      namespace: "Microsoft.UI.Xaml.Controls",
      name: "Grid"
    },
    prefix: "grid"
  },
  {
    owner: {
      namespace: "Microsoft.UI.Xaml.Controls",
      name: "Canvas"
    },
    prefix: "canvas"
  },
  {
    owner: {
      namespace: "Microsoft.UI.Xaml.Controls",
      name: "RelativePanel"
    },
    prefix: "relativePanel"
  },
  {
    owner: {
      namespace: "Microsoft.UI.Xaml.Controls",
      name: "VariableSizedWrapGrid"
    },
    prefix: "variableSizedWrapGrid"
  },
  {
    owner: {
      namespace: "Microsoft.UI.Xaml.Controls",
      name: "ScrollViewer"
    },
    prefix: "scrollViewer"
  },
  {
    owner: {
      namespace: "Microsoft.UI.Xaml.Controls",
      name: "ToolTipService"
    },
    prefix: "toolTip"
  },
  {
    owner: {
      namespace: "Microsoft.UI.Xaml.Automation",
      name: "AutomationProperties"
    },
    prefix: "automation"
  }
];

// What the runtime conversions build or read, which the bindings must hold
// whether or not a property names them.
export const supportClasses: TypeName[] = [
  {
    namespace: "Microsoft.UI.Xaml",
    name: "Application"
  },
  {
    namespace: "Microsoft.UI.Xaml.Controls",
    name: "ColumnDefinition"
  },
  {
    namespace: "Microsoft.UI.Xaml.Controls",
    name: "RowDefinition"
  },
  {
    namespace: "Microsoft.UI.Xaml.Media",
    name: "Brush"
  },
  {
    namespace: "Microsoft.UI.Xaml.Media",
    name: "ImageSource"
  },
  {
    namespace: "Microsoft.UI.Xaml.Media",
    name: "SolidColorBrush"
  },
  {
    namespace: "Microsoft.UI.Xaml.Media",
    name: "FontFamily"
  },
  {
    namespace: "Microsoft.UI.Xaml.Media.Imaging",
    name: "BitmapImage"
  },
  {
    namespace: "Windows.Foundation",
    name: "PropertyValue"
  },
  {
    namespace: "Windows.Foundation",
    name: "Uri"
  }
];

// What the package itself reaches besides its components, from src/winui.ts
// (the package's one import site of the bindings; a test keeps the two in
// step): an app generates bindings for these as well.
export const coreClasses: TypeName[] = [
  {
    namespace: "Microsoft.UI.Dispatching",
    name: "DispatcherQueue"
  },
  {
    namespace: "Microsoft.UI.Dispatching",
    name: "DispatcherQueueTimer"
  },
  {
    namespace: "Microsoft.UI.Windowing",
    name: "AppWindow"
  },
  {
    namespace: "Microsoft.UI.Xaml",
    name: "ResourceDictionary"
  },
  {
    namespace: "Microsoft.UI.Xaml",
    name: "Style"
  },
  {
    namespace: "Microsoft.UI.Xaml",
    name: "Window"
  },
  {
    namespace: "Microsoft.UI.Xaml.Controls",
    name: "RowDefinition"
  },
  {
    namespace: "Microsoft.UI.Xaml.Markup",
    name: "XamlReader"
  },
  {
    namespace: "Microsoft.UI.Xaml.Media",
    name: "DesktopAcrylicBackdrop"
  }
];

// Exports of the bindings that are not metadata types: the generic
// instantiations the conversions name.
export const extraBindings = [
  {
    name: "IReference_Boolean",
    module: "#winui/windows/foundation/IReference_Boolean"
  }
];

// Enums the conversions use, which arrive in the bindings with ColumnDefinition
// (its GridLength names GridUnitType).
export const supportEnums: TypeName[] = [
  {
    namespace: "Microsoft.UI.Xaml",
    name: "GridUnitType"
  }
];

// How a property's value is written, by the type it holds.
export const PropertyKind = {
  Boolean: "boolean",
  Number: "number",
  String: "string",
  Enum: "enum",
  Thickness: "thickness",
  CornerRadius: "cornerRadius",
  GridLength: "gridLength",
  Brush: "brush",
  Color: "color",
  ImageSource: "imageSource",
  FontFamily: "fontFamily",
  Uri: "uri",
  NullableBoolean: "nullableBoolean",
  Object: "object",
  Passthrough: "passthrough"
} as const;

export type PropertyKind = (typeof PropertyKind)[keyof typeof PropertyKind];

// By the full name of the type; anything else a value or a class is handed
// on as the bindings type it.
export const kindOfType = new Map<string, PropertyKind>([
  ["Microsoft.UI.Xaml.Thickness", PropertyKind.Thickness],
  ["Microsoft.UI.Xaml.CornerRadius", PropertyKind.CornerRadius],
  ["Microsoft.UI.Xaml.GridLength", PropertyKind.GridLength],
  ["Microsoft.UI.Xaml.Media.Brush", PropertyKind.Brush],
  ["Windows.UI.Color", PropertyKind.Color],
  ["Microsoft.UI.Xaml.Media.ImageSource", PropertyKind.ImageSource],
  ["Microsoft.UI.Xaml.Media.FontFamily", PropertyKind.FontFamily],
  ["Windows.Foundation.Uri", PropertyKind.Uri]
]);

export const numericPrimitives = new Set(["SByte", "Byte", "Int16", "UInt16", "Int32", "UInt32", "Single", "Double"]);
