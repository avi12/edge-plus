<script lang="ts">
  import { ColorModeName, ThemeBadgeName } from "./theme-vocabulary.ts";
  import type { ColorMode, ThemeBadge, ThemeRow } from "#src/protocol.g.ts";
  import { onMount } from "svelte";
  import { cubicOut } from "svelte/easing";
  import { fade } from "svelte/transition";
  import {
    Border,
    Button,
    FluentGlyph,
    Flyout,
    FontIcon,
    Grid,
    MenuFlyout,
    MenuFlyoutItem,
    ProgressRing,
    StackPanel,
    TextBlock
  } from "winui-svelte/components";
  import type { FluentGlyphValue } from "winui-svelte/components";
  import { clipboard, launchUri, revealInExplorer } from "winui-svelte/data-transfer";

  interface Props {
    theme: ThemeRow;
    onremove: (() => void) | null;
    onassign: (mode: ColorMode) => void;
    canReorder?: boolean;
    shouldConfirmRemove?: boolean;
    /** The default theme's row: it cannot be dragged or removed, and keeps its columns in line with the others. */
    isFixed?: boolean;
    /** The row was added just now. */
    isNew?: boolean;
    /** The theme was added to be used at once (Shift): the row waits for Edge to wear it. */
    isUsedNow?: boolean;
  }

  const {
    theme,
    onremove,
    onassign,
    canReorder = false,
    shouldConfirmRemove = false,
    isFixed = false,
    isNew = false,
    isUsedNow = false
  }: Props = $props();

  const BADGE_TEXT: Record<ThemeBadge, string> = {
    store: "Store",
    local: "Local",
    inUse: "In use",
    installedInEdge: "Installed in Edge",
    light: "Light",
    dark: "Dark"
  };
  const MODES: readonly ColorMode[] = [ColorModeName.light, ColorModeName.dark];
  const MODE_TEXT: Record<ColorMode, string> = {
    light: "Light",
    dark: "Dark"
  };
  const MODE_GLYPH: Record<ColorMode, FluentGlyphValue> = {
    light: FluentGlyph.Brightness,
    dark: FluentGlyph.QuietHours
  };
  /** WinUI's normal motion duration. */
  const BADGE_MILLISECONDS = 250;
  /** A badge's line: 18px of badge and its 2px margin. A node WinUI has not laid out yet measures 0. */
  const BADGE_LINE_HEIGHT = 20;
  /** The longest the wait for Edge shows. */
  const PENDING_MILLISECONDS = 4000;
  /** A theme added to be used at once is downloaded, installed and switched to by Edge, which takes longer. */
  const NEW_THEME_PENDING_MILLISECONDS = 30_000;
  /** The shortest it shows, so a fast switch does not flash it. */
  const PENDING_MINIMUM_MILLISECONDS = 600;
  const SHOWN = 1;
  const HIDDEN = 0;
  let isHovered = $state(false);
  let hasFocus = $state(false);
  const isRevealed = $derived(isHovered || hasFocus);
  const revealedOpacity = $derived.by(() => {
    if (isRevealed) {
      return SHOWN;
    }

    return HIDDEN;
  });

  function isAssigned(mode: ColorMode) {
    return theme.badges.includes(mode);
  }

  // The button stays enabled once it is the badge: a disabled control loses focus, and WinUI hands it to the
  // next row's button, which reveals that row.
  function assign(mode: ColorMode) {
    if (!isAssigned(mode)) {
      pendingLimit = PENDING_MILLISECONDS;
      isPending = true;
      onassign(mode);
    }
  }

  // From the click until Edge wears the theme and the row says "In use": a ring stands where the badge
  // will be; a switch for the other Windows mode never says it, so the wait is also cut short.
  let isPending = $state(false);
  let pendingLimit = $state(PENDING_MILLISECONDS);
  let hasShownLongEnough = $state(false);
  onMount(() => {
    const isWaitingFromTheStart = isNew && isUsedNow && !isInUse;
    if (isWaitingFromTheStart) {
      pendingLimit = NEW_THEME_PENDING_MILLISECONDS;
      isPending = true;
    }
  });
  $effect(() => {
    const isSettled = isPending && MODES.some(isAssigned) && isInUse && hasShownLongEnough;
    if (isSettled) {
      isPending = false;
    }
  });
  $effect(() => {
    if (!isPending) {
      return;
    }

    hasShownLongEnough = false;
    const shortest = setTimeout(() => (hasShownLongEnough = true), PENDING_MINIMUM_MILLISECONDS);
    const longest = setTimeout(() => (isPending = false), pendingLimit);
    return () => {
      clearTimeout(shortest);
      clearTimeout(longest);
    };
  });

  // A property, not `style:opacity`: with an opacity transition WinUI fades in a Style-set opacity but
  // leaves it at 1 when the Style is swapped back, so the buttons would never hide again.
  // The badge fades out of the row that loses it; the button of the row that gains it becomes the badge in place.
  function badgeOpacity(mode: ColorMode) {
    if (isAssigned(mode)) {
      return SHOWN;
    }

    return HIDDEN;
  }

  function buttonOpacity(mode: ColorMode) {
    if (isAssigned(mode)) {
      return HIDDEN;
    }

    return revealedOpacity;
  }
  const sideOpacity = $derived.by(() => {
    if (isFixed) {
      return HIDDEN;
    }

    return SHOWN;
  });
  const listedBadges = $derived(
    theme.badges.filter(
      badge => badge !== ThemeBadgeName.light && badge !== ThemeBadgeName.dark && badge !== ThemeBadgeName.inUse
    )
  );
  const isOwn = $derived(onremove !== null && !isFixed);
  const hasOrigin = $derived(theme.origin.length > 0);
  const isInUse = $derived(theme.badges.includes(ThemeBadgeName.inUse));
  const isWaitingForInUse = $derived(isPending && (!isInUse || !hasShownLongEnough));
  // The badge that follows the ring waits for the ring to fade out, so the two never show together.
  let hasShownRing = false;
  $effect(() => {
    if (isWaitingForInUse) {
      hasShownRing = true;
    } else if (!isInUse) {
      hasShownRing = false;
    }
  });
  function inUseDelay() {
    if (hasShownRing) {
      return BADGE_MILLISECONDS;
    }

    return 0;
  }
  const isInUseShown = $derived(isInUse && !isWaitingForInUse);
  const hasBadges = $derived(listedBadges.length > 0 || isInUseShown || isWaitingForInUse);

  /** Svelte's slide, but a node not yet measured opens to a known height instead of not moving. */
  function slideLine(node: Element, { duration }: { duration: number }) {
    const measured = Number.parseFloat(getComputedStyle(node).height);
    const height = Math.max(measured, BADGE_LINE_HEIGHT);
    return {
      duration,
      easing: cubicOut,
      css: (progress: number) => `overflow: hidden; opacity: ${progress}; height: ${progress * height}px`
    };
  }

  /** A Local or Store badge opens where the theme came from; null for the others. */
  function launchOf(badge: ThemeBadge) {
    if (!hasOrigin) {
      return null;
    }

    if (badge === ThemeBadgeName.local) {
      return {
        automationId: `reveal-${theme.id}`,
        automationName: `Show ${theme.name} in File Explorer`,
        copyText: "Copy file path",
        open: () => revealInExplorer(theme.origin)
      };
    }

    if (badge === ThemeBadgeName.store) {
      return {
        automationId: `launch-${theme.id}`,
        automationName: `Open ${theme.name} in the browser`,
        copyText: "Copy link",
        open: () => launchUri(theme.origin)
      };
    }

    return null;
  }
  const confirmDetail = $derived.by(() => {
    if (isInUse) {
      return "This uninstalls the theme from your Edge profile, and Edge goes back to its default theme.";
    }

    return "This uninstalls the theme from your Edge profile.";
  });
  let isConfirmOpen = $state(false);

  function confirmRemove() {
    isConfirmOpen = false;
    onremove?.();
  }
</script>

<Grid
  class="theme-row"
  automationName={theme.name}
  ongotfocus={() => (hasFocus = true)}
  onlostfocus={() => (hasFocus = false)}
  onpointerentered={() => (isHovered = true)}
  onpointerexited={() => (isHovered = false)}
>
  {#if canReorder}
    <Border class="theme-row-grip">
      <FontIcon style:opacity={sideOpacity} class="theme-row-icon" glyph={FluentGlyph.GripperBarHorizontal} />
    </Border>
  {/if}
  <StackPanel class="theme-row-main">
    <Border class="theme-row-name">
      <TextBlock class="theme-row-title" textReadingOrder="DetectFromContent">{theme.name}</TextBlock>
    </Border>
    <!-- The default theme's only badge is "In use": its line slides open and shut, so the rows below glide. -->
    {#if hasBadges}
      <border transition:slideLine={{ duration: BADGE_MILLISECONDS }}>
        <StackPanel class="theme-row-badges">
          {#each listedBadges as badge (badge)}
            {@const launch = launchOf(badge)}
            {#if launch !== null}
              <Button
                class="badge"
                automationId={launch.automationId}
                automationName={launch.automationName}
                onclick={launch.open}
              >
                {#snippet contextFlyout()}
                  <MenuFlyout>
                    <MenuFlyoutItem onclick={() => clipboard.writeText(theme.origin)}>
                      {launch.copyText}
                      {#snippet icon()}
                        <FontIcon glyph={FluentGlyph.Copy} />
                      {/snippet}
                    </MenuFlyoutItem>
                  </MenuFlyout>
                {/snippet}
                <StackPanel class="badge-launch">
                  <TextBlock class="badge-text">{BADGE_TEXT[badge]}</TextBlock>
                  <FontIcon class="badge-launch-icon" glyph={FluentGlyph.OpenInNewWindow} />
                </StackPanel>
              </Button>
            {:else}
              <Border class="badge">
                <TextBlock class="badge-text">{BADGE_TEXT[badge]}</TextBlock>
              </Border>
            {/if}
          {/each}
          <!-- One cell for the ring and the badge, so the ring fades out where "In use" fades in. Each has its
               own block, so the fade plays when it comes or goes, and not when the row is made. -->
          {#if isInUseShown || isWaitingForInUse}
            <Grid class="in-use-slot">
              {#if isWaitingForInUse}
                <border class="in-use-ring" transition:fade|global={{ duration: BADGE_MILLISECONDS }}>
                  <ProgressRing class="in-use-spinner" isActive />
                </border>
              {/if}
              {#if isInUseShown}
                <border
                  class="badge badge-in-use" in:fade|global={{
                    duration: BADGE_MILLISECONDS,
                    delay: inUseDelay()
                  }} out:fade|global={{ duration: BADGE_MILLISECONDS }}>
                  <TextBlock class="badge-in-use-text">{BADGE_TEXT.inUse}</TextBlock>
                </border>
              {/if}
            </Grid>
          {/if}
        </StackPanel>
      </border>
    {/if}
  </StackPanel>
  <StackPanel class="theme-row-modes">
    {#each MODES as mode (mode)}
      <Grid class="mode-slot">
        <Border class="badge badge-in-use mode-badge" opacity={badgeOpacity(mode)}>
          <StackPanel class="mode-content">
            <FontIcon class="mode-badge-icon" glyph={MODE_GLYPH[mode]} />
            <TextBlock class="badge-in-use-text">{MODE_TEXT[mode]}</TextBlock>
          </StackPanel>
        </Border>
        <Button
          class="mode-button"
          automationId="assign-{mode}-{theme.id}"
          automationName="Use {theme.name} when Windows is {mode}"
          onclick={() => assign(mode)}
          opacity={buttonOpacity(mode)}
        >
          <StackPanel class="mode-content">
            <FontIcon class="mode-icon" glyph={MODE_GLYPH[mode]} />
            <TextBlock class="badge-text">{MODE_TEXT[mode]}</TextBlock>
          </StackPanel>
        </Button>
      </Grid>
    {/each}
  </StackPanel>
  {#if shouldConfirmRemove}
    <Button
      style:opacity={sideOpacity}
      class="theme-row-remove"
      automationId="remove-{theme.id}"
      automationName="Remove {theme.name}"
      isEnabled={isOwn}
    >
      <FontIcon class="theme-row-icon" glyph={FluentGlyph.Cancel} />
      {#snippet flyout()}
        <Flyout bind:open={isConfirmOpen}>
          <StackPanel class="confirm">
            <TextBlock class="confirm-title">Remove {theme.name} from Edge?</TextBlock>
            <TextBlock class="confirm-detail">{confirmDetail}</TextBlock>
            <StackPanel class="confirm-buttons">
              <Button automationId="confirm-remove-{theme.id}" onclick={confirmRemove}>Remove</Button>
              <Button automationId="cancel-remove-{theme.id}" onclick={() => (isConfirmOpen = false)}>Cancel</Button>
            </StackPanel>
          </StackPanel>
        </Flyout>
      {/snippet}
    </Button>
  {:else}
    <Button
      style:opacity={sideOpacity}
      class="theme-row-remove"
      automationId="remove-{theme.id}"
      automationName="Remove {theme.name}"
      isEnabled={isOwn}
      onclick={() => onremove?.()}
    >
      <FontIcon class="theme-row-icon" glyph={FluentGlyph.Cancel} />
    </Button>
  {/if}
</Grid>

<style>
  .theme-row {
    grid-template-columns: auto 1fr auto auto;
    padding: 0 4px;
    background: transparent;
  }

  .theme-row-icon {
    font-size: 12px;
  }

  .theme-row-grip {
    align-self: center;
    padding-right: 8px;
    cursor: move;
  }

  .theme-row-main {
    grid-column: 2;
    align-self: center;
    padding: 4px 0;
  }

  .theme-row-title {
    text-wrap: wrap;
    -winui-based-on: BodyTextBlockStyle;
  }

  .theme-row-badges {
    flex-direction: row;
    gap: 4px;
    margin-top: 2px;
  }

  .theme-row-modes {
    flex-direction: row;
    grid-column: 3;
    gap: 4px;
    align-self: center;
    margin-left: 8px;
  }

  .mode-slot {
    width: 72px;
  }

  .mode-badge {
    justify-self: stretch;
    pointer-events: none;
    transition: opacity 150ms;
  }

  .mode-badge-icon {
    align-self: center;
    color: var(--text-on-accent-fill-color-primary);
    font-size: 10px;
  }

  .mode-button {
    justify-self: stretch;
    min-height: 0;
    padding: 1px 6px;
    transition: opacity 150ms;
  }

  .mode-content {
    flex-direction: row;
    gap: 4px;
    align-self: center;
    justify-self: center;
  }

  .theme-row-remove {
    grid-column: 4;
    margin-left: 8px;
  }

  .confirm {
    gap: 8px;
    max-width: 280px;
  }

  .confirm-title {
    -winui-based-on: BodyStrongTextBlockStyle;
  }

  .confirm-detail {
    color: var(--text-fill-color-secondary);
    text-wrap: wrap;
  }

  .confirm-buttons {
    flex-direction: row;
    gap: 8px;
  }

  .badge {
    align-self: center;
    padding: 1px 6px;
    border-radius: var(--control-corner-radius);
    background: var(--subtle-fill-color-secondary);
  }

  .in-use-slot {
    min-width: 44px;
    height: 20px;
  }

  .in-use-spinner {
    width: 16px;
    height: 16px;
    color: var(--text-fill-color-primary);
  }

  .in-use-ring {
    align-self: center;
    justify-self: center;
  }

  .mode-icon {
    align-self: center;
    color: var(--text-fill-color-secondary);
    font-size: 10px;
  }

  .badge-launch {
    flex-direction: row;
    gap: 4px;
  }

  .badge-launch-icon {
    align-self: center;
    color: var(--text-fill-color-secondary);
    font-size: 10px;
  }

  .badge-in-use {
    background: var(--accent-fill-color-default);
  }

  .badge-in-use-text {
    color: var(--text-on-accent-fill-color-primary);
    -winui-based-on: CaptionTextBlockStyle;
  }

  .badge-text {
    color: var(--text-fill-color-secondary);
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
