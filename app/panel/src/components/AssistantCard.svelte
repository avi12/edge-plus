<script lang="ts">
  import CardHeader from "@/components/CardHeader.svelte";
  import Select from "@/components/Select.svelte";
  import { Assistant } from "@/lib/bridge";

  interface Props {
    assistant: Assistant;
    description: string;
    disabled?: boolean;
    icon: string;
    onChange: (assistant: Assistant) => void;
    title: string;
  }

  const { assistant, description, disabled = false, icon, onChange, title }: Props = $props();

  const options = [
    {
      value: Assistant.Copilot,
      label: "Copilot (Edge's default)"
    },
    {
      value: Assistant.Gemini,
      label: "Gemini"
    }
  ];

  function choose(value: string): void {
    onChange(value === Assistant.Gemini ? Assistant.Gemini : Assistant.Copilot);
  }
</script>

<section class="card">
  <CardHeader {description} {icon} {title}>
    <div class="control" class:disabled>
      <Select label={title} onchange={choose} {options} value={assistant} />
    </div>
  </CardHeader>
</section>

<style>
  .card {
    padding: var(--space-l);
    border: 1px solid var(--stroke-2);
    border-radius: 8px;
    background: var(--bg-card);
    box-shadow: var(--shadow-card);
  }

  .control {
    flex: 0 0 auto;
  }

  .control.disabled {
    opacity: 50%;
    pointer-events: none;
  }
</style>
