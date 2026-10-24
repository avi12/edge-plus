using System;
using System.Linq;

namespace EdgePlus;

internal static class PanelLayoutTests {
  public static void Run() {
    CardSlot[] cards = PanelLayout.AllCards().ToArray();
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      Expect.Equal(1, cards.Count(card => card.Feature == feature), $"{feature} has one card");
    }
    foreach (OwnCard own in Enum.GetValues<OwnCard>()) {
      Expect.Equal(1, cards.Count(card => card.Own == own), $"{own} has one card");
      Expect.That(Wording.For(own).Title.Length > 0, $"{own} has words");
    }
    Expect.That(cards.All(card => card.Feature.HasValue != card.Own.HasValue),
                "each card is a feature's or Edge+'s own, not both");

    Section[] sections = PanelLayout.Sections.Select(each => each.Section).ToArray();
    Expect.That(sections.SequenceEqual(sections.Distinct()), "each section appears once");
    foreach (Section section in sections) {
      Expect.That(Wording.SectionName(section).Length > 0, $"{section} has a name");
    }
    Expect.That(sections.SequenceEqual(Enum.GetValues<Section>()), "every section is shown");

    // A dependent card sits right under the card it needs, or under another
    // card that needs the same one, in the same section.
    foreach ((Section section, CardSlot[] sectionCards) in PanelLayout.Sections) {
      for (int i = 0; i < sectionCards.Length; i++) {
        if (sectionCards[i].Needs is not Feature needed) {
          continue;
        }
        int at = i - 1;
        while (at >= 0 && sectionCards[at].Needs == needed) {
          at--;
        }
        bool isUnderItsFeature = at >= 0 && sectionCards[at].Feature == needed;
        Expect.That(isUnderItsFeature, $"{sectionCards[i].Feature} sits under {needed}");
      }
    }
    Expect.Equal("Turn on Theme by Windows light and dark to use this.",
                 Wording.NeedsFeature(Feature.Theming), "the dependency hint");
  }
}
