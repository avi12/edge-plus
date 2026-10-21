using System.Collections.Generic;

namespace EdgePlus;

// The window's sections, top to bottom, in the Windows Settings style: a short
// header over a stack of cards. Each section's name is in Wording.
internal enum Section {
  TabsAndWindows,
  LookAndThemes,
  Browsing,
  EdgePlusItself,
}

// One card in a section: a feature's switch (the theming feature's card is the
// theming card), or the start-with-Windows card, which is Edge+'s own and no
// feature. A card that only works while another feature is on names it in
// `Needs`: it sits indented right under that feature's card and is held, with
// a short hint, while that one is off.
internal readonly record struct CardSlot(Feature? Feature, Feature? Needs = null) {
  public static CardSlot StartWithWindows => new(null);

  public bool IsStartWithWindows => Feature is null;
}

// The one definition of which sections there are, in what order, and which
// cards each holds, in what order. PanelView builds the window from it.
internal static class PanelLayout {
  public static readonly (Section Section, CardSlot[] Cards)[] Sections = [
    (Section.TabsAndWindows, [
      new(Feature.NativeSplit),
      new(Feature.LivePreview),
    ]),
    (Section.LookAndThemes, [
      new(Feature.Theming),
      // The spine is drawn as part of the theme (spine.h), so it is under it.
      new(Feature.TabSpine, Needs: Feature.Theming),
      new(Feature.WebUiFollowsTheme),
      new(Feature.ReadingTheme),
    ]),
    (Section.Browsing, [
      new(Feature.EnhancedFind),
      new(Feature.ProtocolHandlers),
      // An Edge prompt, so with Edge's behaviour, not with Edge+'s own settings.
      new(Feature.QuietDevMode),
    ]),
    // Edge+'s own settings only.
    (Section.EdgePlusItself, [
      CardSlot.StartWithWindows,
    ]),
  ];

  // Every card, in window order.
  public static IEnumerable<CardSlot> AllCards() {
    foreach ((Section _, CardSlot[] cards) in Sections) {
      foreach (CardSlot card in cards) {
        yield return card;
      }
    }
  }
}
