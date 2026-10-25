import { SvelteSet } from "svelte/reactivity";

// Window-only state, with Svelte's own tools: a class holding $state and a
// SvelteSet. Nothing here knows about the host.
export class Tallies {
  localCount = $state(0);
  readonly buttonsUsed = new SvelteSet<string>();
  readonly doubled = $derived(this.localCount * 2);

  bump(button: string): void {
    this.localCount++;
    this.buttonsUsed.add(button);
  }
}
