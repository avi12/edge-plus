//! The window's cards: each feature's switch and Edge+'s own. Which section
//! each sits in, in which order and saying what, is the window's markup
//! (`ui/window/page/PanelCards.svelte`).

use crate::platform::settings::Feature;

/// Edge+'s own cards: about Edge+ itself, not a feature it adds to Edge.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) enum OwnCard {
    StartWithWindows,
    AutoInject,
    AutoUpdate,
}

/// A card is either a feature's switch or one of Edge+'s own, never both. A
/// feature card that only works while another feature is on names it in
/// `needs`, and is held, with a short hint, while that one is off.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum CardSlot {
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

pub(crate) const CARDS: [CardSlot; 12] = [
    feature(Feature::NativeSplit),
    feature(Feature::LivePreview),
    feature(Feature::Theming),
    // The skin's tab colours are read with the theme's (cyberpunk_skin.h).
    CardSlot::Feature {
        feature: Feature::CyberpunkSkin,
        needs: Some(Feature::Theming),
    },
    feature(Feature::WebUiFollowsTheme),
    feature(Feature::ReadingTheme),
    feature(Feature::EnhancedFind),
    feature(Feature::ProtocolHandlers),
    feature(Feature::QuietDevMode),
    CardSlot::Own(OwnCard::StartWithWindows),
    CardSlot::Own(OwnCard::AutoInject),
    CardSlot::Own(OwnCard::AutoUpdate),
];

pub(crate) fn all_cards() -> impl Iterator<Item = CardSlot> {
    CARDS.into_iter()
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
