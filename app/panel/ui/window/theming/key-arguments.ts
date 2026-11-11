import type { ComponentProps } from "svelte";
import { projectAs } from "winui-svelte/bindings/lifetime";
import { KeyRoutedEventArgs } from "winui-svelte/bindings/microsoft/ui/xaml/input/KeyRoutedEventArgs";
import type { Border } from "winui-svelte/components";

type KeyHandler = NonNullable<ComponentProps<typeof Border>["onpreviewkeydown"]>;
export type KeyEventArguments = Parameters<KeyHandler>[1];
export type KeyHandlerSender = Parameters<KeyHandler>[0];

/** The key event of a preview key handler as WinUI's `KeyRoutedEventArgs`: its `key`, and `handled` to settle it. */
export function keyEventOf(args: KeyEventArguments) {
  return projectAs(args, KeyRoutedEventArgs);
}
