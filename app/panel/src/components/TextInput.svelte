<script lang="ts">
  interface Props {
    onEnter?: () => void;
    onInput: (value: string) => void;
    placeholder?: string;
    value: string;
  }

  const { onEnter, onInput, placeholder = "", value }: Props = $props();

  function handleInput(event: Event): void {
    if (event.currentTarget instanceof HTMLInputElement) {
      onInput(event.currentTarget.value);
    }
  }

  function handleKeydown(event: KeyboardEvent): void {
    if (event.key === "Enter" && onEnter) {
      onEnter();
    }
  }
</script>

<input
  class="field input"
  oninput={handleInput}
  onkeydown={handleKeydown}
  {placeholder}
  type="text"
  {value}
/>

<style>
  .input::placeholder {
    color: var(--fg-3);
  }

  .input:focus {
    border-color: var(--brand);
    border-bottom-width: 2px;
    outline: none;
  }
</style>
