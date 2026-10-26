import * as bindings from "../src/components/generated/bindings.ts";
import type { LevelMetadata } from "../src/components/generated/metadata.ts";
import { levels } from "../src/components/generated/metadata.ts";
import ToggleSwitch from "../src/components/generated/ToggleSwitch.svelte";
import type { NativeValue } from "../src/native-value.ts";
import { mountControl } from "../src/renderer/host/mount.svelte.ts";
import { read } from "../src/renderer/host/reflect.ts";
import type { Counts, Failure, Report } from "./mount-report.ts";
import { DispatcherQueue } from "#winui/microsoft/ui/dispatching/DispatcherQueue";
import { Application } from "#winui/microsoft/ui/xaml/Application";
import { Window } from "#winui/microsoft/ui/xaml/Window";
import { appendFileSync } from "node:fs";
import { flushSync } from "svelte";

// The inside of the mount test (mount-all.ts): on the UI thread, every
// generated component is linked to a control it makes, first with no props
// and then with every property at the value it has, every event with a
// handler and every attached property, so a constructor, a property setter or
// an event subscription that throws is found. Then a few behaviours are
// checked: a bindable goes both ways, a changed prop is written again, and
// the conversions build what WinUI takes.

const offScreen = {
  x: -8000,
  y: 0,
  width: 700,
  height: 4000
};
const layoutMilliseconds = 1500;
const ticksPerMillisecond = 10_000n;
const attachedPrefix = "set";
// WinUI refuses the composite transform properties (rotation, scale, …) on an
// element whose RenderTransform, its origin, Projection or Transform3D is in
// use, so the sweep, which writes back every default, leaves these out.
const renderTransformFamily = new Set(["renderTransform", "renderTransformOrigin", "projection", "transform3D"]);
// What WinUI refuses to be set on a control, which the sweep therefore leaves
// out: AnimatedVisualPlayer owns its visual, RichTextBlockOverflow takes its
// tab behaviour from its RichTextBlock.
const compositeTransformFamily = ["centerPoint", "rotation", "rotationAxis", "scale", "transformMatrix"];
// Controls made, mounted and set like the rest, but WinUI fails fast (a native
// crash, no exception) when one is laid out in a plain panel: each draws
// itself for an owner that hands it its parts (a list view's item, a Pivot, a
// map), so the test does not place them.
const needsOwnerToLayOut = new Set([
  "GridViewItemPresenter",
  "ListViewItemPresenter",
  "MapControl",
  "PivotPanel",
  "RevealListViewItemPresenter",
  "WrapGrid"
]);
const refusedByWinui = new Map<string, string[]>([
  ["AnimatedVisualPlayer", compositeTransformFamily],
  ["RichTextBlockOverflow", ["isTabStop", "tabIndex"]]
]);

type Linked = () => void;
type Connect = (control: object, props: object, bound: object) => Linked;

// Held until the process ends: a timer whose wrapper is collected stops.
// eslint-disable-next-line svelte/prefer-svelte-reactivity -- not component state
const heldTimers = new Set<object>();

// A native failure takes the process down without a message, so the last
// component started is written, synchronously, where this variable says.
const traceFile = process.env["WINUI_SVELTE_TRACE"];

function trace(line: string): void {
  if (traceFile !== undefined) {
    appendFileSync(traceFile, `${Date.now() % 1_000_000} ${line}\n`);
  }
}

function isConstructor(value: NativeValue): value is new () => object {
  return typeof value === "function";
}

function isElement(value: object): value is bindings.UIElement {
  return "opacity" in value;
}

function isConnect(value: NativeValue): value is Connect {
  return typeof value === "function";
}

function chainOf(name: string): LevelMetadata[] {
  const chain: LevelMetadata[] = [];
  let level: LevelMetadata | undefined = levels[name];
  for (; level !== undefined; level = level.parent === null ? undefined : levels[level.parent]) {
    chain.push(level);
  }

  return chain;
}

function componentNames(): string[] {
  return Object.entries(levels).filter(([, level]) => level.isComponent).map(([name]) => name);
}

async function loadConnectors(): Promise<Map<string, Connect>> {
  // eslint-disable-next-line svelte/prefer-svelte-reactivity -- not component state
  const connectors = new Map<string, Connect>();
  for (const name of componentNames()) {
    const loaded: Record<string, NativeValue> = await import(`../src/components/generated/controls/${name}.svelte.ts`);
    const connect = loaded[`connect${name}`];
    if (!isConnect(connect)) {
      throw new Error(`${name} has no connect${name}`);
    }

    connectors.set(name, connect);
  }

  return connectors;
}

// What a control would be given for each key: the value it has now (null
// ones are left out, as they have nothing to write back).
interface Sample {
  props: Record<string, NativeValue>;
  state: Record<string, NativeValue>;
  counts: Counts;
}

function attachedValue(owner: string, setter: string, control: object): NativeValue {
  const getter = `get${setter.slice(attachedPrefix.length)}`;
  const holder = read(bindings, owner);
  const method = read(holder, getter);
  return typeof method === "function" ? method.apply(holder, [control]) : undefined;
}

function samplePropertiesOf(
  sample: Sample,
  name: string,
  level: LevelMetadata,
  control: object,
  isWanted: (key: string) => boolean
): void {
  for (const key of level.properties) {
    const value = read(control, key);
    const isBindable = level.bindables.includes(key);
    const isWritable = !renderTransformFamily.has(key) && !refusedByWinui.get(name)?.includes(key);
    const hasValue = value !== undefined && value !== null && isWanted(key) && isWritable;
    const isBoundValue = hasValue && isBindable;
    if (isBoundValue) {
      sample.state[key] = value;
      sample.counts.bindables++;
    } else if (hasValue) {
      sample.props[key] = value;
      sample.counts.properties++;
    }
  }
}

function sampleAttachedOf(
  sample: Sample,
  level: LevelMetadata,
  control: object,
  isWanted: (key: string) => boolean
): void {
  for (const [key, [owner, setter]] of Object.entries(level.attached).filter(([key]) => isWanted(key))) {
    const value = attachedValue(owner, setter, control);
    const isPresent = value !== undefined && value !== null;
    if (isPresent) {
      sample.props[key] = value;
      sample.counts.attached++;
    }
  }
}

function sampleOf(name: string, control: object, only: ReadonlySet<string> | null): Sample {
  const sample = emptySample();
  function isWanted(key: string): boolean {
    return only === null || only.has(key);
  }

  for (const level of chainOf(name)) {
    samplePropertiesOf(sample, name, level, control, isWanted);
    for (const key of level.events.filter(isWanted)) {
      sample.props[key] = () => undefined;
      sample.counts.events++;
    }

    sampleAttachedOf(sample, level, control, isWanted);
  }

  return sample;
}

// The props the component gets, and the bindables as getters and setters over
// state the test holds, as the component's own destructuring would.
function boundOver(state: Record<string, NativeValue>, name: string): object {
  const bound: Record<string, NativeValue> = {};
  for (const level of chainOf(name)) {
    for (const key of level.bindables) {
      Object.defineProperty(bound, key, {
        get: () => state[key],
        set(value) {
          state[key] = value;
        },
        enumerable: true
      });
    }
  }

  return bound;
}

function unlinkNothing(): void {}

// Links the control inside an effect root, as the component's attachment
// would; what it returns ends the root and the subscriptions.
function open(connect: Connect, control: object, props: object, bound: object): () => void {
  let unlink: Linked = unlinkNothing;
  const stop = $effect.root(() => {
    unlink = connect(control, props, bound);
  });
  return () => {
    stop();
    unlink();
  };
}

function link(connect: Connect, name: string, control: object, sample: Sample): void {
  const close = open(connect, control, sample.props, boundOver(sample.state, name));
  try {
    flushSync();
  } finally {
    close();
  }
}

function emptySample(): Sample {
  return {
    props: {},
    state: {},
    counts: {
      properties: 0,
      events: 0,
      attached: 0,
      bindables: 0
    }
  };
}

function describe(value: NativeValue): string {
  return typeof value === "object" && value !== null ? JSON.stringify(value, (_key, part) => typeof part === "bigint" ? String(part) : part) : String(value);
}

class Exercise {
  readonly failures: Failure[] = [];
  readonly exercised: Counts = {
    properties: 0,
    events: 0,
    attached: 0,
    bindables: 0
  };
  mounted = 0;
  private readonly panel: bindings.StackPanel;

  constructor(panel: bindings.StackPanel) {
    this.panel = panel;
  }

  private attempt(name: string, step: string, action: () => void): boolean {
    try {
      action();
      return true;
    } catch (error) {
      this.failures.push({
        name,
        step,
        message: error instanceof Error ? error.message : String(error)
      });
      return false;
    }
  }

  component(name: string, connect: Connect): void {
    trace(`${name}`);
    const Control = read(bindings, name);
    if (!isConstructor(Control)) {
      this.failures.push({
        name,
        step: "bindings",
        message: "the bindings export no class of this name"
      });
      return;
    }

    let control: object | null = null;
    const isMade = this.attempt(name, "constructor", () => {
      control = new Control();
    });
    if (!isMade) {
      return;
    }

    if (control === null) {
      return;
    }

    const isLinked = this.attempt(name, "default props", () => link(connect, name, new Control(), emptySample()));
    // Every value is the control's own default, so an object it holds (its
    // resources, its transitions) goes back to the control that holds it.
    const made: object = control;
    const sample = sampleOf(name, made, null);
    const isFull = this.attempt(name, "all props", () => link(connect, name, made, sample));
    if (isFull) {
      this.exercised.properties += sample.counts.properties;
      this.exercised.events += sample.counts.events;
      this.exercised.attached += sample.counts.attached;
      this.exercised.bindables += sample.counts.bindables;
    } else {
      this.bisect(name, connect, Control, sample);
    }

    const isFullyMounted = isLinked && isFull;
    if (isFullyMounted) {
      this.mounted++;
    }

    const isPlaced = new RegExp(process.env["WINUI_SVELTE_PLACE"] ?? ".").test(name) && !needsOwnerToLayOut.has(name);
    // A text element (Run, Bold) is placed in a text block, not a panel.
    const canPlace = isPlaced && isElement(made);
    if (canPlace) {
      const element = made;
      this.attempt(name, "place in a panel", () => this.panel.children.append(element));
    }
  }

  // Which key made the full mount fail: each one alone, and when every one
  // alone is fine, the smallest set that still fails together (the keys
  // dropped one at a time while it fails).
  private bisect(name: string, connect: Connect, Control: new () => object, sample: Sample): void {
    const keys = [...Object.keys(sample.props), ...Object.keys(sample.state)];
    function fails(subset: string[]): boolean {
      const control = new Control();
      try {
        link(connect, name, control, sampleOf(name, control, new Set(subset)));
        return false;
      } catch {
        return true;
      }
    }

    let isAnyAloneFailing = false;
    for (const key of keys) {
      const control = new Control();
      const isFine = this.attempt(name, `${key} = ${describe(sample.props[key] ?? sample.state[key])}`, () => link(connect, name, control, sampleOf(name, control, new Set([key]))));
      isAnyAloneFailing = isAnyAloneFailing || !isFine;
    }

    if (!isAnyAloneFailing) {
      let minimal = keys;
      for (const key of keys) {
        const without = minimal.filter(candidate => candidate !== key);
        if (fails(without)) {
          minimal = without;
        }
      }

      this.failures.push({
        name,
        step: "together",
        message: `these fail together, though each alone is fine: ${minimal.join(", ")}`
      });
    }
  }
}

function checkBehaviours(connectors: Map<string, Connect>, failures: Failure[]): number {
  let count = 0;
  function check(name: string, action: () => void): void {
    count++;
    try {
      action();
    } catch (error) {
      failures.push({
        name: `behaviour: ${name}`,
        step: "check",
        message: error instanceof Error ? error.message : String(error)
      });
    }
  }

  function connectorFor(name: string): Connect {
    const connect = connectors.get(name);
    if (connect === undefined) {
      throw new Error(`no component ${name}`);
    }

    return connect;
  }

  function expect(isTrue: boolean, message: string): void {
    if (!isTrue) {
      throw new Error(message);
    }
  }

  check("a bindable goes both ways (ToggleSwitch.isOn)", () => {
    const toggle = new bindings.ToggleSwitch();
    const state = $state<Record<string, NativeValue>>({ isOn: true });
    const close = open(connectorFor("ToggleSwitch"), toggle, {}, boundOver(state, "ToggleSwitch"));
    flushSync();
    expect(toggle.isOn, "the state's true did not reach the switch");
    toggle.isOn = false;
    expect(state["isOn"] === false, "the user's flip did not reach the state");
    state["isOn"] = true;
    flushSync();
    expect(toggle.isOn, "the state's change did not reach the switch");
    close();
  });

  check("a bindable number goes both ways (Slider.value)", () => {
    const slider = new bindings.Slider();
    const state = $state<Record<string, NativeValue>>({ value: 30 });
    const close = open(connectorFor("Slider"), slider, {}, boundOver(state, "Slider"));
    flushSync();
    expect(slider.value === 30, "the state's value did not reach the slider");
    slider.value = 70;
    expect(state["value"] === 70, "the user's value did not reach the state");
    close();
  });

  check("a boolean-or-null bindable is written (CheckBox.isChecked)", () => {
    const box = new bindings.CheckBox();
    const state = $state<Record<string, NativeValue>>({ isChecked: true });
    const close = open(connectorFor("CheckBox"), box, {}, boundOver(state, "CheckBox"));
    flushSync();
    expect(box.isChecked === true, `the state's true did not reach the box (it reads ${String(box.isChecked)})`);
    close();
  });

  check("a changed prop is written again (Border.opacity)", () => {
    const border = new bindings.Border();
    const props = $state<Record<string, NativeValue>>({ opacity: 0.5 });
    const close = open(connectorFor("Border"), border, props, {});
    flushSync();
    expect(border.opacity === 0.5, "the first value was not written");
    props["opacity"] = 0.25;
    flushSync();
    expect(border.opacity === 0.25, "the changed value was not written");
    close();
  });

  check("an event prop is called (ToggleSwitch.onToggled)", () => {
    const toggle = new bindings.ToggleSwitch();
    let calls = 0;
    function onToggled(): void {
      calls++;
    }

    const close = open(connectorFor("ToggleSwitch"), toggle, { onToggled }, {});
    flushSync();
    toggle.isOn = true;
    expect(calls === 1, `the handler was called ${calls} times, not once`);
    close();
  });

  check("conversions build what WinUI takes (Border)", () => {
    const border = new bindings.Border();
    const props = {
      padding: [8, 4],
      cornerRadius: 6,
      background: "#336699",
      horizontalAlignment: "Center",
      width: 120,
      gridRow: 2,
      attached: [bindings.Canvas, "Left", 7]
    };
    const close = open(connectorFor("Border"), border, props, {});
    flushSync();
    expect(border.padding.left === 8 && border.padding.top === 4, "padding [8, 4] is not left/right 8, top/bottom 4");
    expect(border.cornerRadius.topLeft === 6, "cornerRadius 6 is not 6 on every corner");
    expect(border.horizontalAlignment === 1, "horizontalAlignment Center is not 1");
    expect(border.width === 120, "width was not written");
    expect(bindings.Grid.getRow(border) === 2, "gridRow 2 did not reach Grid.Row");
    expect(bindings.Canvas.getLeft(border) === 7, "attached Canvas.Left 7 did not reach Canvas.Left");
    expect(border.background !== null, "the background color made no brush");
    close();
  });

  check("a string is boxed for an object property (Button.content)", () => {
    const button = new bindings.Button();
    const close = open(connectorFor("Button"), button, { content: "OK" }, {});
    flushSync();
    expect(button.content !== null, "the content was not written");
    close();
  });

  return count;
}

function statistics(): Pick<Report, "components" | "properties" | "events" | "attached" | "bindables"> {
  const all = Object.values(levels);
  function sum(pick: (level: LevelMetadata) => number): number {
    return all.reduce((total, level) => total + pick(level), 0);
  }

  return {
    components: componentNames().length,
    properties: sum(level => level.properties.length),
    events: sum(level => level.events.length),
    attached: sum(level => Object.keys(level.attached).length),
    bindables: sum(level => level.bindables.length)
  };
}

function launch(
  connectors: Map<string, Connect>,
  report: (result: Report) => void,
  throughRenderer: (failures: Failure[]) => void
): void {
  const panel = new bindings.StackPanel();
  const window = new Window();
  window.content = panel;
  const exercise = new Exercise(panel);
  const only = new RegExp(process.env["WINUI_SVELTE_ONLY"] ?? ".");
  for (const [name, connect] of connectors) {
    if (only.test(name)) {
      exercise.component(name, connect);
    }
  }

  trace("behaviours");
  const behaviours = checkBehaviours(connectors, exercise.failures) + 1;
  throughRenderer(exercise.failures);
  trace("show");
  window.appWindow.moveAndResize(offScreen);
  window.appWindow.showWithActivation(false);
  trace("shown");
  // Let WinUI measure and arrange what was placed before the verdict.
  const timer = DispatcherQueue.getForCurrentThread().createTimer();
  timer.interval = { duration: BigInt(layoutMilliseconds) * ticksPerMillisecond };
  timer.isRepeating = false;
  timer.onTick(() => {
    const result: Report = {
      ...statistics(),
      mounted: exercise.mounted,
      exercised: exercise.exercised,
      behaviours,
      failures: exercise.failures
    };
    report(result);
    Application.current.exit();
  });
  heldTimers.add(timer);
  timer.start();
}

// A generated .svelte component through the renderer, as an app would use it.
function checkComponentThroughRenderer(failures: Failure[]): void {
  try {
    let toggles = 0;
    const mounted = mountControl(ToggleSwitch, {
      isOn: true,
      header: "Hi",
      onToggled() {
        toggles++;
      }
    });
    if (read(mounted.root, "isOn") !== true) {
      throw new Error("<ToggleSwitch isOn> did not turn the switch on");
    }

    mounted.set({ isOn: false });

    if (read(mounted.root, "isOn") !== false) {
      throw new Error("a changed isOn prop did not reach the switch");
    }

    if (toggles === 0) {
      throw new Error("onToggled was not called when the prop turned the switch off");
    }
  } catch (error) {
    failures.push({
      name: "behaviour: ToggleSwitch.svelte through the renderer",
      step: "check",
      message: error instanceof Error ? error.message : String(error)
    });
  }
}

export async function run(report: (result: Report) => void): Promise<void> {
  trace("loading");
  const connectors = await loadConnectors();
  trace("loaded");
  await Application.startScheduled(() => {
    Application.create(() => launch(connectors, report, checkComponentThroughRenderer));
  });
}
