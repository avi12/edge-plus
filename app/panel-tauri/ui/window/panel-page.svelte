<script lang="ts">
  import type { FooterView, SectionView, SwitchKey } from "../protocol.g.ts";
  import { milliseconds } from "./layout.ts";
  import * as motion from "./motion.ts";
  import { withControl } from "./svelte-renderer.ts";
  import type { FrameworkElement } from "./winui.ts";
  import {
    DoubleCollection,
    InfoBarSeverity,
    ScalarTransition,
    ScrollBarVisibility,
    StackPanel,
    Uri,
    Visibility
  } from "./winui.ts";

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

  const veilShown = 1;
  const veilHidden = 0;
  const veilDashes = [4, 3];
  const veilFadeMilliseconds = 120;

  const veilFade = new ScalarTransition();
  veilFade.duration = milliseconds(veilFadeMilliseconds);
  const veilDashArray = new DoubleCollection();
  for (const dash of veilDashes) {
    veilDashArray.append(dash);
  }

  // Motion follows Windows' Animation effects switch, so motion.ts holds each
  // panel rather than markup setting its transitions.
  function animated(transitions: typeof motion.reposition): (node: unknown) => void {
    return withControl(StackPanel, panel => {
      motion.animate(panel, transitions);
    });
  }
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
          <stack-panel spacing="4" {@attach animated(motion.reposition)}>
            <!-- The left margin lines the header up with the cards' text, as
                 Windows Settings does. -->
            <text-block class="SectionHeader" margin="4,0,0,4">{section.name}</text-block>
            {#each section.cards as card (card.key)}
              {#if card.inject === null}
                <host element={cardControl(card.key)} />
              {:else}
                <!-- Its add/delete transition slides the Inject card in and
                     out. -->
                <stack-panel spacing="4" {@attach animated(motion.addDelete)}>
                  <host element={cardControl(card.key)} />
                  {#if injectCard !== null}
                    <host element={injectCard} />
                  {/if}
                </stack-panel>
              {/if}
            {/each}
          </stack-panel>
        {/each}
      </stack-panel>
      <grid margin="4,14,4,0" columns="*,Auto">
        <stack-panel orientation="Horizontal" spacing="4">
          <text-block class="CardDescription" vertical-alignment="Center">{footer.by}</text-block>
          <hyperlink-button navigate-uri={new Uri(footer.authorUri)} padding="2,0,2,0">{footer.author}</hyperlink-button>
        </stack-panel>
        <text-block class="CardDescription" vertical-alignment="Center" grid.column={1}>{footer.version}</text-block>
      </grid>
    </stack-panel>
  </scroll-viewer>
  <grid class="DropVeil" margin="12" opacity={isVeilShown ? veilShown : veilHidden} is-hit-test-visible="false" opacity-transition={veilFade}>
    <rectangle class="DropVeilFrame" stroke-dash-array={veilDashArray} radius-x="8" radius-y="8" />
    <text-block class="CardTitle" horizontal-alignment="Center" vertical-alignment="Center">{dropHint}</text-block>
  </grid>
</grid>
