<script lang="ts">
  import CardHeader from "@/components/CardHeader.svelte";
  import Switch from "@/components/Switch.svelte";

  interface Props {
    checked: boolean;
    description: string;
    disabled?: boolean;
    icon: string;
    note?: string;
    onchange: (value: boolean) => void;
    title: string;
  }

  const { checked, description, disabled = false, icon, note = "", onchange, title }: Props = $props();
</script>

<section class="card">
  <CardHeader {description} {icon} {note} {title}>
    <Switch {checked} {disabled} label={title} {onchange} />
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
</style>
