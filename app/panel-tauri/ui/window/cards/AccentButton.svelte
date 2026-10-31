<script lang="ts">
  import type { Snippet } from "svelte";
  import { Button } from "winui-svelte/components";
  import type { ButtonComponentProps } from "winui-svelte/components";

  interface Props extends ButtonComponentProps {
    children: Snippet;
  }

  const { children, class: className, ...rest }: Props = $props();
</script>

<Button class={["accent-button", className]} {...rest}>{@render children()}</Button>

<style>
  .accent-button {
    -winui-based-on: AccentButtonStyle;
  }
</style>
