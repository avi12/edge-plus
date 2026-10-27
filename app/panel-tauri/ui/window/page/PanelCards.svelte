<script lang="ts">
  import type { CardView, SwitchKey, View } from "../../protocol.g.ts";
  import FeatureCard from "../cards/FeatureCard.svelte";
  import InjectCard from "../cards/InjectCard.svelte";
  import * as motion from "../style/motion.ts";
  import ThemingBody from "../theming/ThemingBody.svelte";
  import PanelSection from "./PanelSection.svelte";
  import SectionHeader from "./SectionHeader.svelte";
  import { StackPanel } from "winui-svelte";

  // Every section and card, in order, and what each says: the View carries
  // only their state. A feature card's title is shared with the core's toast
  // (wording/shared-wording.json); "RegExp" and "Copilot" must match the DLL's
  // names for them (find_text.h, chat_target.h).
  interface Props {
    view: View;
  }

  const { view }: Props = $props();

  function cardOf(key: SwitchKey): CardView {
    const card = view.cards.find(candidate => candidate.key === key);
    if (card === undefined) {
      throw new Error(`the core sent no ${key} card`);
    }

    return card;
  }
</script>

<StackPanel class="sections" childrenTransitions={motion.applied(motion.reposition)}>
  <PanelSection>
    <SectionHeader>Tabs and windows</SectionHeader>
    <FeatureCard
      card={cardOf("nativeSplit")}
      description="Grows Edge's own split past two panes: add panes on any side, drag the dividers, move with Shift+Alt+arrow. Live, no restart. Open Edge first."
      glyph={0xF0E2}
    />
    <FeatureCard
      card={cardOf("livePreview")}
      description="Hover a tab and its thumbnail plays live instead of a frozen snapshot, by keeping Edge's own tab capture running while the preview is open. Live, no restart. Open Edge first."
      glyph={0xE890}
    />
  </PanelSection>
  <PanelSection>
    <SectionHeader>Look and themes</SectionHeader>
    <FeatureCard
      card={cardOf("theming")}
      description="Use one theme when Windows is light and another when it is dark, switched live. Add themes from the Chrome Web Store, Edge Add-ons or a local file."
      glyph={0xE790}
    >
      {#snippet body()}
        <ThemingBody isOn={cardOf("theming").isOn} theming={view.theming} />
      {/snippet}
    </FeatureCard>
    <!-- The cut corners are cut from the theme's tab fill, so it is under it. -->
    <FeatureCard
      card={cardOf("tabShape")}
      description="Gives every tab, vertical and horizontal, a straight leading edge and a cut trailing corner, with the selected tab filled in the theme's neon. Works with Theming on. Live, no restart."
      glyph={0xE7C3}
    />
    <FeatureCard
      card={cardOf("webUiFollowsTheme")}
      description="Makes edge://settings and edge://extensions light or dark to match your Edge theme, not Windows. Only those two pages change; websites are untouched. Live, no restart. Open Edge first."
      glyph={0xE706}
    />
    <FeatureCard
      card={cardOf("readingTheme")}
      description="Reading mode uses one page theme while Windows is light and another while it is dark, switched as Windows switches. Pick both in reading mode: Appearance settings, Themes, then Light mode or Dark mode. Live, no restart."
      glyph={0xE736}
    />
  </PanelSection>
  <PanelSection>
    <SectionHeader>Browsing</SectionHeader>
    <FeatureCard
      card={cardOf("enhancedFind")}
      description="Adds RegExp (with its flags) and Ask Copilot to the options of Edge's Ctrl+F bar. RegExp highlights every match; Ask sends your question to Copilot, which opens in its side panel. Live, no restart. Open Edge first."
      glyph={0xE721}
    />
    <FeatureCard
      card={cardOf("protocolHandlers")}
      description="Like Chrome: when a site such as Gmail offers to open email or calendar links, closing the bubble no longer blocks it for good; only Ignore or Block does. Turning this on lifts earlier blocks. Live, no restart."
      glyph={0xE71B}
    />
    <!-- An Edge prompt, so with Edge's behaviour, not with Edge+'s own
         settings. -->
    <FeatureCard
      card={cardOf("quietDevMode")}
      description="Stops Edge asking to turn off developer-mode extensions when it starts. Takes effect from Edge's next start."
      glyph={0xE7BA}
    />
  </PanelSection>
  <PanelSection>
    <SectionHeader>Edge+ itself</SectionHeader>
    <FeatureCard
      card={cardOf("startWithWindows")}
      description="Runs Edge+ in the background, with no window, from when you sign in to Windows: your settings apply and an Edge update that needs a newer Edge+ is caught. Launch Edge+ to open its window."
      glyph={0xE7E8}
      title="Start Edge+ when Windows starts"
    />
    <!-- Its add/delete transition slides the Inject card in and out. -->
    <StackPanel class="inject-cards" childrenTransitions={motion.applied(motion.addDelete)}>
      <FeatureCard
        card={cardOf("autoInject")}
        description="Puts Edge+ into Edge each time Edge starts. Off, Edge starts without it and Edge+ offers an Inject button."
        glyph={0xE945}
        title="Inject automatically"
      />
      {#if view.inject !== "hidden"}
        <InjectCard offer={view.inject} />
      {/if}
    </StackPanel>
    <FeatureCard
      card={cardOf("autoUpdate")}
      description="Looks for a new Edge+ release on GitHub and offers to restart into it. Off, Edge+ never contacts GitHub."
      glyph={0xE895}
      title="Check for updates"
    />
  </PanelSection>
</StackPanel>

<!-- Global: a class on a component is not an element's, so Svelte does not
     scope it. -->
<style>
  :global(.sections) {
    gap: 20px;
  }

  :global(.inject-cards) {
    gap: 4px;
  }
</style>
