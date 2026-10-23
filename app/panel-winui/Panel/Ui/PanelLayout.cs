using System.Collections.Generic;

namespace EdgePlus;

internal enum Section {
  TabsAndWindows,
  LookAndThemes,
  Browsing,
  EdgePlusItself,
}

// A null Feature is the start-with-Windows card, Edge+'s own. A card that only
// works while another feature is on names it in `Needs`, and is held, with a
// short hint, while that one is off.
internal readonly record struct CardSlot(Feature? Feature, Feature? Needs = null) {
  public static CardSlot StartWithWindows => new(null);

  public bool IsStartWithWindows => Feature is null;
}

internal static class PanelLayout {
  public static readonly (Section Section, CardSlot[] Cards)[] Sections = [
    (Section.TabsAndWindows, [
      new(Feature.NativeSplit),
      new(Feature.LivePreview),
    ]),
    (Section.LookAndThemes, [
      new(Feature.Theming),
      // The cut corners are cut from the theme's tab fill (tab_shape.h), so it is under it.
      new(Feature.TabShape, Needs: Feature.Theming),
      new(Feature.WebUiFollowsTheme),
      new(Feature.ReadingTheme),
    ]),
    (Section.Browsing, [
      new(Feature.EnhancedFind),
      new(Feature.ProtocolHandlers),
      // An Edge prompt, so with Edge's behaviour, not with Edge+'s own settings.
      new(Feature.QuietDevMode),
    ]),
    (Section.EdgePlusItself, [
      CardSlot.StartWithWindows,
    ]),
  ];

  public static IEnumerable<CardSlot> AllCards() {
    foreach ((Section _, CardSlot[] cards) in Sections) {
      foreach (CardSlot card in cards) {
        yield return card;
      }
    }
  }
}
