//! The window's sections and the cards in each, in order.

use crate::platform::settings::Feature;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Section {
    TabsAndWindows,
    LookAndThemes,
    Browsing,
    EdgePlusItself,
}

/// Edge+'s own cards: about Edge+ itself, not a feature it adds to Edge.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum OwnCard {
    StartWithWindows,
    AutoInject,
    AutoUpdate,
}

/// A card is either a feature's switch or one of Edge+'s own, never both. A
/// feature card that only works while another feature is on names it in
/// `needs`, and is held, with a short hint, while that one is off.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CardSlot {
    Feature {
        feature: Feature,
        needs: Option<Feature>,
    },
    Own(OwnCard),
}

const fn feature(feature: Feature) -> CardSlot {
    CardSlot::Feature {
        feature,
        needs: None,
    }
}

pub const SECTIONS: [(Section, &[CardSlot]); 4] = [
    (
        Section::TabsAndWindows,
        &[feature(Feature::NativeSplit), feature(Feature::LivePreview)],
    ),
    (
        Section::LookAndThemes,
        &[
            feature(Feature::Theming),
            // The cut corners are cut from the theme's tab fill (tab_shape.h),
            // so it is under it.
            CardSlot::Feature {
                feature: Feature::TabShape,
                needs: Some(Feature::Theming),
            },
            feature(Feature::WebUiFollowsTheme),
            feature(Feature::ReadingTheme),
        ],
    ),
    (
        Section::Browsing,
        &[
            feature(Feature::EnhancedFind),
            feature(Feature::ProtocolHandlers),
            // An Edge prompt, so with Edge's behaviour, not with Edge+'s own
            // settings.
            feature(Feature::QuietDevMode),
        ],
    ),
    (
        Section::EdgePlusItself,
        &[
            CardSlot::Own(OwnCard::StartWithWindows),
            CardSlot::Own(OwnCard::AutoInject),
            CardSlot::Own(OwnCard::AutoUpdate),
        ],
    ),
];

pub fn all_cards() -> impl Iterator<Item = CardSlot> {
    SECTIONS.iter().flat_map(|(_, cards)| cards.iter().copied())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_feature_has_exactly_one_card() {
        for wanted in Feature::ALL {
            let count = all_cards()
                .filter(
                    |slot| matches!(slot, CardSlot::Feature { feature, .. } if *feature == wanted),
                )
                .count();
            assert_eq!(1, count, "{wanted:?}");
        }
    }

    #[test]
    fn every_own_card_appears_once() {
        for wanted in [
            OwnCard::StartWithWindows,
            OwnCard::AutoInject,
            OwnCard::AutoUpdate,
        ] {
            let count = all_cards()
                .filter(|slot| *slot == CardSlot::Own(wanted))
                .count();
            assert_eq!(1, count, "{wanted:?}");
        }
    }

    #[test]
    fn a_needed_feature_is_never_itself() {
        for slot in all_cards() {
            if let CardSlot::Feature {
                feature,
                needs: Some(needed),
            } = slot
            {
                assert_ne!(feature, needed);
            }
        }
    }
}
