<script lang="ts">
  import CardHeader from "@/components/CardHeader.svelte";
  import Select from "@/components/Select.svelte";
  import Switch from "@/components/Switch.svelte";
  import { Mode } from "@/lib/bridge";
  import type { ReadingMode, ReadingTheme } from "@/lib/bridge";
  import { READING_THEMES, readingThemeOf, swatchIcon } from "@/lib/readingThemes";

  interface Props {
    description: string;
    disabled?: boolean;
    enabled: boolean;
    icon: string;
    note?: string;
    onSetMode: (mode: Mode, theme: ReadingTheme) => Promise<void>;
    onToggle: (value: boolean) => void;
    readingMode: ReadingMode;
  }

  const {
    description,
    disabled = false,
    enabled,
    icon,
    note = "",
    onSetMode,
    onToggle,
    readingMode
  }: Props = $props();

  const TITLE = "Reading mode follows Windows light and dark";

  const options = READING_THEMES.map(info => ({
    value: info.theme,
    label: info.label,
    icon: swatchIcon(info.color)
  }));

  function setMode(mode: Mode, value: string): void {
    const theme = readingThemeOf(value);
    if (theme !== undefined) {
      void onSetMode(mode, theme);
    }
  }
</script>

<section class="card">
  <CardHeader {description} {icon} {note} title={TITLE}>
    <Switch checked={enabled} {disabled} label={TITLE} onchange={onToggle} />
  </CardHeader>

  <div class="card-panel" class:dimmed={!enabled}>
    <div class="assign">
      <span class="assign-label">When Windows is light</span>
      <Select
        label="Reading mode theme when Windows is light"
        onchange={value => setMode(Mode.Light, value)}
        {options}
        value={readingMode.light}
      />
    </div>
    <div class="assign">
      <span class="assign-label">When Windows is dark</span>
      <Select
        label="Reading mode theme when Windows is dark"
        onchange={value => setMode(Mode.Dark, value)}
        {options}
        value={readingMode.dark}
      />
    </div>
  </div>
</section>

<style>
  .assign {
    display: flex;
    gap: var(--space-m);
    justify-content: space-between;
    align-items: center;
  }

  .assign + .assign {
    margin-top: var(--space-m);
  }

  .assign-label {
    font-size: 13px;
  }
</style>
