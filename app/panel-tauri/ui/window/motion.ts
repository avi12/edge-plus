import type { ListView } from "./winui.ts";
import {
  AddDeleteThemeTransition,
  ReorderThemeTransition,
  RepositionThemeTransition,
  TransitionCollection,
  UISettings
} from "./winui.ts";
import { createSubscriber } from "svelte/reactivity";

// The one place that honours Windows' "Animation effects" switch: a panel's
// `childrenTransitions={applied(reposition)}` gets WinUI's theme transitions
// only while it is on, and refresh() re-reads it live (the prop follows, since
// applied() reads the switch reactively).

type Transitions = () => TransitionCollection;

const lists: Array<[ListView, Transitions]> = [];
// Made on first use, once the UI thread's apartment exists.
let settings: UISettings | undefined;
let isOn = isSwitchOn();

function ignoreSwitch(): void {
  // Nothing reads the switch yet.
}

let announceSwitch = ignoreSwitch;
const followSwitch = createSubscriber(update => {
  announceSwitch = update;
  return () => {
    announceSwitch = ignoreSwitch;
  };
});

function isSwitchOn(): boolean {
  settings ??= new UISettings();
  return settings.animationsEnabled;
}

// What a panel gets while the switch is as it is now; an effect that calls it
// runs again when the switch flips.
export function applied(transitions: Transitions): TransitionCollection {
  followSwitch();
  return isOn ? transitions() : new TransitionCollection();
}

export function addDelete(): TransitionCollection {
  const collection = new TransitionCollection();
  collection.append(new AddDeleteThemeTransition());
  return collection;
}

export function reposition(): TransitionCollection {
  const collection = new TransitionCollection();
  collection.append(new RepositionThemeTransition());
  return collection;
}

export function listItems(): TransitionCollection {
  const collection = new TransitionCollection();
  collection.append(new AddDeleteThemeTransition());
  collection.append(new ReorderThemeTransition());
  collection.append(new RepositionThemeTransition());
  return collection;
}

export function animateItems(list: ListView, transitions: Transitions): void {
  lists.push([list, transitions]);
  list.itemContainerTransitions = applied(transitions);
}

// Read again whenever the window is activated: the switch is flipped in
// Windows Settings, so the window is always activated again afterwards.
export function refresh(): void {
  const isNowOn = isSwitchOn();
  if (isNowOn === isOn) {
    return;
  }

  isOn = isNowOn;
  announceSwitch();
  for (const [list, transitions] of lists) {
    list.itemContainerTransitions = applied(transitions);
  }
}
