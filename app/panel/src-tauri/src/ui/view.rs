//! Turns the state the actions return into the View the window renders: each
//! card's state, the theme library, the build and the offers. What the window
//! says about them is its own markup.

use crate::edge::version::edge_support::FeatureState;
use crate::edge::version::file_version::FileVersion;
use crate::platform::identity::{app_identity, product};
use crate::platform::settings::{Feature, ThemeSource};
use crate::setup::update_watch::ReadyUpdate;
use crate::themes::theme_order::{self, Listed};
use crate::ui::inject_offer::InjectOffer;
use crate::ui::panel_layout::{self, CardSlot, OwnCard};
use crate::ui::panel_state::PanelState;
use crate::ui::protocol::{
    CardHold, CardView, EdgeBuild, FooterView, SwitchKey, ThemeBadge, ThemeOption, ThemeRow,
    ThemingView, UpdateView, View,
};

impl EdgeBuild {
    #[must_use]
    pub(crate) fn of(edge: FileVersion, supported: FileVersion) -> Self {
        Self {
            version: edge.to_string(),
            is_updated: edge > supported,
        }
    }
}

#[must_use]
pub(crate) fn switch_key(slot: CardSlot) -> SwitchKey {
    match slot {
        CardSlot::Feature { feature, .. } => match feature {
            Feature::NativeSplit => SwitchKey::NativeSplit,
            Feature::Theming => SwitchKey::Theming,
            Feature::QuietDevMode => SwitchKey::QuietDevMode,
            Feature::ProtocolHandlers => SwitchKey::ProtocolHandlers,
            Feature::EnhancedFind => SwitchKey::EnhancedFind,
            Feature::WebUiFollowsTheme => SwitchKey::WebUiFollowsTheme,
            Feature::LivePreview => SwitchKey::LivePreview,
            Feature::CyberpunkSkin => SwitchKey::CyberpunkSkin,
            Feature::ReadingTheme => SwitchKey::ReadingTheme,
        },
        CardSlot::Own(OwnCard::StartWithWindows) => SwitchKey::StartWithWindows,
        CardSlot::Own(OwnCard::AutoInject) => SwitchKey::AutoInject,
        CardSlot::Own(OwnCard::AutoUpdate) => SwitchKey::AutoUpdate,
    }
}

/// The card a switch key names.
#[must_use]
pub(crate) fn slot_of(key: SwitchKey) -> Option<CardSlot> {
    panel_layout::all_cards().find(|&slot| switch_key(slot) == key)
}

#[must_use]
pub(crate) fn build(state: &PanelState, offer: InjectOffer, update: Option<&ReadyUpdate>) -> View {
    View {
        product_name: product::NAME.to_owned(),
        app_user_model_id: app_identity::APP_ID.to_owned(),
        icon: app_identity::icon_path().to_string_lossy().into_owned(),
        is_edge_running: state.is_edge_running,
        edge: edge_build(state),
        paused: state.paused(),
        cards: panel_layout::all_cards()
            .map(|slot| card(state, slot))
            .collect(),
        theming: theming(state),
        inject: offer,
        update: update.map(|ready| UpdateView {
            version: ready.version.clone(),
        }),
        footer: FooterView {
            author: product::AUTHOR.to_owned(),
            author_uri: product::AUTHOR_URI.to_owned(),
            version: product::VERSION.to_owned(),
        },
    }
}

fn edge_build(state: &PanelState) -> Option<EdgeBuild> {
    state
        .edge_build
        .map(|edge| EdgeBuild::of(edge, state.supported_edge_build))
}

fn card(state: &PanelState, slot: CardSlot) -> CardView {
    let key = switch_key(slot);
    match slot {
        CardSlot::Own(own) => CardView {
            key,
            is_on: match own {
                OwnCard::StartWithWindows => state.is_start_with_windows,
                OwnCard::AutoInject => state.is_auto_inject,
                OwnCard::AutoUpdate => state.is_auto_update,
            },
            is_locked: false,
            hold: None,
        },
        CardSlot::Feature { feature, needs } => {
            // A paused switch shows the user's choice (on), locked, with why;
            // so does one whose needed feature is off.
            let is_needed_off = needs.is_some_and(|needed| !state.features.is_on(needed));
            let needs_hold = needs
                .filter(|_| is_needed_off)
                .map(|needed| CardHold::Needs { feature: needed });
            CardView {
                key,
                is_on: state.features.is_on(feature),
                is_locked: state.is_locked(feature) || is_needed_off,
                hold: build_hold(state, feature).or(needs_hold),
            }
        }
    }
}

/// Held by the build: said only while the build is known.
fn build_hold(state: &PanelState, feature: Feature) -> Option<CardHold> {
    let edge = edge_build(state)?;
    match state.state_of(feature) {
        FeatureState::Paused => Some(CardHold::Paused { edge }),
        FeatureState::Unavailable => Some(CardHold::Unavailable { edge }),
        FeatureState::On | FeatureState::Off => None,
    }
}

fn theme_row(id: &str, name: &str, badges: Vec<ThemeBadge>, origin: &str) -> ThemeRow {
    ThemeRow {
        id: id.to_owned(),
        name: name.to_owned(),
        badges,
        origin: origin.to_owned(),
    }
}

const DEFAULT_THEME_NAME: &str = "Default theme";

/// The Windows modes the theme is set for, then where it came from (Edge's
/// default theme comes from nowhere), then In use last, so that it comes and
/// goes without moving the others. The id is empty for the default theme.
fn badges_of(state: &PanelState, id: &str, source: Option<ThemeBadge>) -> Vec<ThemeBadge> {
    let mut badges = Vec::new();
    if state.theming.light == id {
        badges.push(ThemeBadge::Light);
    }
    if state.theming.dark == id {
        badges.push(ThemeBadge::Dark);
    }
    badges.extend(source);
    if state.worn_theme.as_deref() == Some(id) {
        badges.push(ThemeBadge::InUse);
    }
    badges
}

fn theming(state: &PanelState) -> ThemingView {
    let listed = theme_order::combined(
        &state.theming.themes,
        &state.edge_themes,
        &state.theming.order,
    );
    let options = listed
        .iter()
        .map(|theme| match theme {
            Listed::Own(own) => ThemeOption {
                id: own.id.clone(),
                name: own.name.clone(),
            },
            Listed::Edge(edge) => ThemeOption {
                id: edge.id.clone(),
                name: edge.name.clone(),
            },
        })
        .collect();
    let themes = listed
        .iter()
        .map(|theme| match theme {
            Listed::Own(own) => {
                let source = match own.source {
                    ThemeSource::Store => ThemeBadge::Store,
                    ThemeSource::Local => ThemeBadge::Local,
                };
                let badges = badges_of(state, &own.id, Some(source));
                theme_row(&own.id, &own.name, badges, &own.origin)
            }
            Listed::Edge(edge) => {
                let badges = badges_of(state, &edge.id, Some(ThemeBadge::InstalledInEdge));
                theme_row(&edge.id, &edge.name, badges, "")
            }
        })
        .collect();
    ThemingView {
        options,
        light: state.theming.light.clone(),
        dark: state.theming.dark.clone(),
        default_theme: theme_row("", DEFAULT_THEME_NAME, badges_of(state, "", None), ""),
        themes,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::edge::version::edge_support::Support;
    use crate::platform::settings::{Features, Theming};

    const SUPPORTED: FileVersion = FileVersion::new(155, 0, 4283, 45);
    const UPDATED: FileVersion = FileVersion::new(156, 0, 1, 0);

    fn state(choice: Features, edge: Option<FileVersion>) -> PanelState {
        PanelState {
            features: choice,
            theming: Theming::default(),
            edge_themes: Vec::new(),
            worn_theme: None,
            is_edge_running: edge.is_some(),
            is_start_with_windows: false,
            is_split_loaded: false,
            is_split_wanted: true,
            is_auto_inject: true,
            is_auto_update: true,
            edge_build: edge,
            supported_edge_build: SUPPORTED,
            support: if edge == Some(UPDATED) {
                Support::Refused
            } else {
                Support::Full
            },
        }
    }

    fn hold_of(view: &View, key: SwitchKey) -> Option<CardHold> {
        view.cards
            .iter()
            .find(|card| card.key == key)
            .and_then(|card| card.hold.clone())
    }

    #[test]
    fn every_switch_key_names_one_card() {
        for slot in panel_layout::all_cards() {
            assert_eq!(Some(slot), slot_of(switch_key(slot)));
        }
    }

    #[test]
    fn a_held_card_says_why() {
        let mut choice = Features::none();
        choice.native_split = true;
        choice.cyberpunk_skin = true;
        let updated = build(&state(choice, Some(UPDATED)), InjectOffer::Hidden, None);
        let edge = EdgeBuild {
            version: "156.0.1.0".to_owned(),
            is_updated: true,
        };
        assert_eq!(
            Some(CardHold::Paused { edge: edge.clone() }),
            hold_of(&updated, SwitchKey::NativeSplit)
        );
        assert_eq!(
            Some(CardHold::Unavailable { edge }),
            hold_of(&updated, SwitchKey::LivePreview)
        );
        assert_eq!(None, hold_of(&updated, SwitchKey::QuietDevMode));
        assert_eq!(
            vec![Feature::NativeSplit, Feature::CyberpunkSkin],
            updated.paused
        );

        let supported = build(&state(choice, Some(SUPPORTED)), InjectOffer::Hidden, None);
        assert_eq!(None, hold_of(&supported, SwitchKey::NativeSplit));
        assert_eq!(
            Some(CardHold::Needs {
                feature: Feature::Theming
            }),
            hold_of(&supported, SwitchKey::CyberpunkSkin)
        );
        let cyberpunk_skin = supported
            .cards
            .iter()
            .find(|card| card.key == SwitchKey::CyberpunkSkin)
            .expect("a Cyberpunk skin card");
        assert!(cyberpunk_skin.is_on && cyberpunk_skin.is_locked);
    }

    #[test]
    fn an_unread_build_holds_nothing() {
        let view = build(&state(Features::default(), None), InjectOffer::Hidden, None);
        assert_eq!(None, view.edge);
        assert_eq!(Vec::<Feature>::new(), view.paused);
        assert_eq!(None, hold_of(&view, SwitchKey::NativeSplit));
    }
}
