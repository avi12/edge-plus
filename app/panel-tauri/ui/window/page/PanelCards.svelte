<script lang="ts">
  import PanelSection from "./PanelSection.svelte";
  import SectionHeader from "./SectionHeader.svelte";
  import type { SwitchKey, View } from "#src/protocol.g.ts";
  import CardDescription from "#src/window/cards/CardDescription.svelte";
  import CardTitle from "#src/window/cards/CardTitle.svelte";
  import FeatureCard from "#src/window/cards/FeatureCard.svelte";
  import InjectCard from "#src/window/cards/InjectCard.svelte";
  import ThemingBody from "#src/window/theming/ThemingBody.svelte";
  import { featureTitles } from "#src/window/wording/shared-wording.ts";
  import { AddDeleteThemeTransition, RepositionThemeTransition, StackPanel } from "winui-svelte/components";

  interface Props {
    view: View;
  }

  const { view }: Props = $props();

  function cardOf(key: SwitchKey) {
    const card = view.cards.find(candidate => candidate.key === key);
    const isMissing = card === undefined;
    if (isMissing) {
      throw new Error(`the core sent no ${key} card`);
    }

    return card;
  }
</script>

<StackPanel class="sections">
  {#snippet childrenTransitions()}
    <RepositionThemeTransition />
  {/snippet}
  <PanelSection>
    <SectionHeader>Tabs and windows</SectionHeader>
    <FeatureCard card={cardOf("nativeSplit")}>
      <CardTitle>{featureTitles.nativeSplit}</CardTitle>
      <CardDescription>
        Grows Edge's own split past two panes: add panes on any side, drag the dividers, move with
        Shift+Alt+arrow. Live, no restart. Open Edge first.
      </CardDescription>
    </FeatureCard>
    <FeatureCard card={cardOf("livePreview")}>
      <CardTitle>{featureTitles.livePreview}</CardTitle>
      <CardDescription>
        Hover a tab and its thumbnail plays live instead of a frozen snapshot, by keeping Edge's own tab
        capture running while the preview is open. Live, no restart. Open Edge first.
      </CardDescription>
    </FeatureCard>
  </PanelSection>
  <PanelSection>
    <SectionHeader>Look and themes</SectionHeader>
    <FeatureCard card={cardOf("theming")}>
      <CardTitle>{featureTitles.theming}</CardTitle>
      <CardDescription>
        Use one theme when Windows is light and another when it is dark, switched live. Add themes from the
        Chrome Web Store, Edge Add-ons or a local file.
      </CardDescription>
      {#snippet body()}
        <ThemingBody isOn={cardOf("theming").isOn} theming={view.theming} />
      {/snippet}
    </FeatureCard>
    <FeatureCard card={cardOf("tabShape")}>
      <CardTitle>{featureTitles.tabShape}</CardTitle>
      <CardDescription>
        Gives every tab, vertical and horizontal, a straight leading edge and a cut trailing corner, with
        the selected tab filled in the theme's neon. Works with Theming on. Live, no restart.
      </CardDescription>
    </FeatureCard>
    <FeatureCard card={cardOf("webUiFollowsTheme")}>
      <CardTitle>{featureTitles.webUiFollowsTheme}</CardTitle>
      <CardDescription>
        Makes edge://settings and edge://extensions light or dark to match your Edge theme, not Windows.
        Only those two pages change; websites are untouched. Live, no restart. Open Edge first.
      </CardDescription>
    </FeatureCard>
    <FeatureCard card={cardOf("readingTheme")}>
      <CardTitle>{featureTitles.readingTheme}</CardTitle>
      <CardDescription>
        Reading mode uses one page theme while Windows is light and another while it is dark, switched as
        Windows switches. Pick both in reading mode: Appearance settings, Themes, then Light mode or Dark
        mode. Live, no restart.
      </CardDescription>
    </FeatureCard>
  </PanelSection>
  <PanelSection>
    <SectionHeader>Browsing</SectionHeader>
    <FeatureCard card={cardOf("enhancedFind")}>
      <CardTitle>{featureTitles.enhancedFind}</CardTitle>
      <CardDescription>
        Adds RegExp (with its flags) and Ask Copilot to the options of Edge's Ctrl+F bar. RegExp highlights
        every match; Ask sends your question to Copilot, which opens in its side panel. Live, no restart.
        Open Edge first.
      </CardDescription>
    </FeatureCard>
    <FeatureCard card={cardOf("protocolHandlers")}>
      <CardTitle>{featureTitles.protocolHandlers}</CardTitle>
      <CardDescription>
        Like Chrome: when a site such as Gmail offers to open email or calendar links, closing the bubble
        no longer blocks it for good; only Ignore or Block does. Turning this on lifts earlier blocks. Live,
        no restart.
      </CardDescription>
    </FeatureCard>
    <FeatureCard card={cardOf("quietDevMode")}>
      <CardTitle>{featureTitles.quietDevMode}</CardTitle>
      <CardDescription>
        Stops Edge asking to turn off developer-mode extensions when it starts. Takes effect from Edge's
        next start.
      </CardDescription>
    </FeatureCard>
  </PanelSection>
  <PanelSection>
    <SectionHeader>Edge+ itself</SectionHeader>
    <FeatureCard card={cardOf("startWithWindows")}>
      <CardTitle>Start Edge+ when Windows starts</CardTitle>
      <CardDescription>
        Runs Edge+ in the background, with no window, from when you sign in to Windows: your settings apply
        and an Edge update that needs a newer Edge+ is caught. Launch Edge+ to open its window.
      </CardDescription>
    </FeatureCard>
    <StackPanel class="inject-cards">
      {#snippet childrenTransitions()}
        <AddDeleteThemeTransition />
      {/snippet}
      <FeatureCard card={cardOf("autoInject")}>
        <CardTitle>Inject automatically</CardTitle>
        <CardDescription>
          Puts Edge+ into Edge each time Edge starts. Off, Edge starts without it and Edge+ offers an
          Inject button.
        </CardDescription>
      </FeatureCard>
      {#if view.inject !== "hidden"}
        <InjectCard offer={view.inject} />
      {/if}
    </StackPanel>
    <FeatureCard card={cardOf("autoUpdate")}>
      <CardTitle>Check for updates</CardTitle>
      <CardDescription>
        Looks for a new Edge+ release on GitHub and offers to restart into it. Off, Edge+ never contacts
        GitHub.
      </CardDescription>
    </FeatureCard>
  </PanelSection>
</StackPanel>

<style>
  .sections {
    gap: 20px;
  }

  .inject-cards {
    gap: 4px;
  }
</style>
