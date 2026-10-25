<script lang="ts">
  import type { FooterView, SectionView, SwitchKey } from "../protocol.g.ts";
  import { animated } from "./animated.ts";
  import DropVeil from "./drop-veil.svelte";
  import * as motion from "./motion.ts";
  import PageFooter from "./page-footer.svelte";
  import PanelSection from "./panel-section.svelte";
  import type { FrameworkElement } from "./winui.ts";
  import { InfoBarSeverity, ScrollBarVisibility, Visibility } from "./winui.ts";

  // The window's content: the update card, the notices, each section's cards
  // and the footer, with the veil a drag of files over the window shows.
  interface Props {
    sections: SectionView[];
    // The control for a card, made once per key by the page's code.
    cardControl: (key: SwitchKey) => FrameworkElement;
    updateCard: FrameworkElement;
    // Shown under the Inject automatically card while the core offers it.
    injectCard: FrameworkElement | null;
    openEdgeHint: string | null;
    buildNotice: string;
    actionNotice: string;
    footer: FooterView;
    dropHint: string;
    isVeilShown: boolean;
    onDragOver: (sender: unknown, event: unknown) => void;
    onDragLeave: () => void;
    onDrop: (sender: unknown, event: unknown) => void;
    onActionNoticeClosed: () => void;
  }

  const {
    sections,
    cardControl,
    updateCard,
    injectCard,
    openEdgeHint,
    buildNotice,
    actionNotice,
    footer,
    dropHint,
    isVeilShown,
    onDragOver,
    onDragLeave,
    onDrop,
    onActionNoticeClosed
  }: Props = $props();
</script>

<grid allow-drop="true" ondragover={onDragOver} ondragleave={onDragLeave} ondrop={onDrop}>
  <scroll-viewer vertical-scroll-bar-visibility={ScrollBarVisibility.Auto}>
    <stack-panel padding="16,4,16,16" spacing="4">
      <host element={updateCard} />
      <text-block class="CardDescription" margin="4,0,4,4" visibility={openEdgeHint === null ? Visibility.Collapsed : Visibility.Visible}>{openEdgeHint ?? ""}</text-block>
      <info-bar is-closable="false" severity={InfoBarSeverity.Warning} message={buildNotice} is-open={buildNotice.length > 0} />
      <info-bar is-closable="true" severity={InfoBarSeverity.Error} message={actionNotice} is-open={actionNotice.length > 0} onclosed={onActionNoticeClosed} />
      <stack-panel spacing="20" {@attach animated(motion.reposition)}>
        {#each sections as section (section.name)}
          <PanelSection {section} {cardControl} {injectCard} />
        {/each}
      </stack-panel>
      <PageFooter {footer} />
    </stack-panel>
  </scroll-viewer>
  <DropVeil hint={dropHint} isShown={isVeilShown} />
</grid>
