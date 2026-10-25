//! Turns the state the actions return into the View the window renders: which
//! cards, in which sections, saying what.

use crate::platform::product;
use crate::platform::settings::{Feature, ThemeSource};
use crate::setup::update_watch::ReadyUpdate;
use crate::ui::inject_offer::InjectOffer;
use crate::ui::panel_layout::{self, CardSlot, OwnCard};
use crate::ui::panel_state::PanelState;
use crate::ui::protocol::{
    CardView, CardWording, FooterView, InjectSlot, InjectView, SectionView, SwitchKey, ThemeOption,
    ThemeRow, ThemingLabels, ThemingView, UpdateView, View,
};
use crate::ui::wording;

impl From<wording::CardWording> for CardWording {
    fn from(wording: wording::CardWording) -> Self {
        Self {
            title: wording.title,
            description: wording.description,
            glyph: wording.glyph,
        }
    }
}

#[must_use]
pub fn switch_key(slot: CardSlot) -> SwitchKey {
    match slot {
        CardSlot::Feature { feature, .. } => match feature {
            Feature::NativeSplit => SwitchKey::NativeSplit,
            Feature::Theming => SwitchKey::Theming,
            Feature::QuietDevMode => SwitchKey::QuietDevMode,
            Feature::ProtocolHandlers => SwitchKey::ProtocolHandlers,
            Feature::EnhancedFind => SwitchKey::EnhancedFind,
            Feature::WebUiFollowsTheme => SwitchKey::WebUiFollowsTheme,
            Feature::LivePreview => SwitchKey::LivePreview,
            Feature::TabShape => SwitchKey::TabShape,
            Feature::ReadingTheme => SwitchKey::ReadingTheme,
        },
        CardSlot::Own(OwnCard::StartWithWindows) => SwitchKey::StartWithWindows,
        CardSlot::Own(OwnCard::AutoInject) => SwitchKey::AutoInject,
        CardSlot::Own(OwnCard::AutoUpdate) => SwitchKey::AutoUpdate,
    }
}

/// The card a switch key names.
#[must_use]
pub fn slot_of(key: SwitchKey) -> Option<CardSlot> {
    panel_layout::all_cards().find(|&slot| switch_key(slot) == key)
}

#[must_use]
pub fn build(state: &PanelState, offer: InjectOffer, update: Option<&ReadyUpdate>) -> View {
    let build_notice = state.edge_build.map_or_else(String::new, |edge| {
        wording::describe_paused(edge, state.supported_edge_build, &state.paused())
    });
    View {
        product_name: product::NAME.to_owned(),
        open_edge_hint: (!state.is_edge_running).then(|| wording::OPEN_EDGE_HINT.to_owned()),
        build_notice,
        sections: panel_layout::SECTIONS
            .iter()
            .map(|(section, cards)| SectionView {
                name: wording::section_name(*section).to_owned(),
                cards: cards.iter().map(|&slot| card(state, slot, offer)).collect(),
            })
            .collect(),
        update: update.map(|ready| UpdateView {
            wording: wording::update_ready(&ready.version, ready.is_installer).into(),
            button: wording::UPDATE_BUTTON.to_owned(),
            restarting: wording::UPDATE_RESTARTING.to_owned(),
        }),
        footer: FooterView {
            by: wording::BY.to_owned(),
            author: product::AUTHOR.to_owned(),
            author_uri: product::AUTHOR_URI.to_owned(),
            version: product::VERSION.to_owned(),
        },
        drop_hint: wording::DROP_HINT.to_owned(),
    }
}

fn card(state: &PanelState, slot: CardSlot, offer: InjectOffer) -> CardView {
    let key = switch_key(slot);
    match slot {
        CardSlot::Own(own) => CardView {
            key,
            wording: wording::for_own(own).into(),
            is_on: match own {
                OwnCard::StartWithWindows => state.is_start_with_windows,
                OwnCard::AutoInject => state.is_auto_inject,
                OwnCard::AutoUpdate => state.is_auto_update,
            },
            is_locked: false,
            note: String::new(),
            theming: None,
            inject: (own == OwnCard::AutoInject).then(|| InjectSlot {
                offer: inject_view(offer),
            }),
        },
        CardSlot::Feature { feature, needs } => {
            // A paused switch shows the user's choice (on), locked, with why;
            // so does one whose needed feature is off.
            let is_needed_off = needs.is_some_and(|needed| !state.features.is_on(needed));
            let mut note = wording::describe_feature(state, feature);
            if let Some(needed) = needs.filter(|_| note.is_empty() && is_needed_off) {
                note = wording::needs_feature(needed);
            }
            CardView {
                key,
                wording: wording::for_feature(feature).into(),
                is_on: state.features.is_on(feature),
                is_locked: state.is_locked(feature) || is_needed_off,
                note,
                theming: (feature == Feature::Theming).then(|| theming(state)),
                inject: None,
            }
        }
    }
}

fn inject_view(offer: InjectOffer) -> Option<InjectView> {
    let wording = match offer {
        InjectOffer::Hidden => return None,
        InjectOffer::Ready => wording::inject_ready(),
        InjectOffer::Injected => wording::injected(),
    };
    Some(InjectView {
        wording: wording.into(),
        button: wording::INJECT_BUTTON.to_owned(),
        is_enabled: offer == InjectOffer::Ready,
    })
}

fn theme_row(id: &str, name: &str, badges: Vec<&str>) -> ThemeRow {
    ThemeRow {
        id: id.to_owned(),
        name: name.to_owned(),
        badges: badges.into_iter().map(str::to_owned).collect(),
        remove_label: wording::remove_theme(name),
    }
}

fn theming(state: &PanelState) -> ThemingView {
    let mut options = vec![ThemeOption {
        id: String::new(),
        label: wording::DEFAULT_THEME.to_owned(),
    }];
    options.extend(state.theming.themes.iter().map(|theme| ThemeOption {
        id: theme.id.clone(),
        label: theme.name.clone(),
    }));
    options.extend(state.edge_themes.iter().map(|theme| ThemeOption {
        id: theme.id.clone(),
        label: theme.name.clone(),
    }));
    let own: Vec<ThemeRow> = state
        .theming
        .themes
        .iter()
        .map(|theme| {
            let badge = match theme.source {
                ThemeSource::Store => wording::STORE_BADGE,
                ThemeSource::Local => wording::LOCAL_BADGE,
            };
            theme_row(&theme.id, &theme.name, vec![badge])
        })
        .collect();
    let edge: Vec<ThemeRow> = state
        .edge_themes
        .iter()
        .map(|theme| {
            let badges = if theme.is_active {
                vec![wording::IN_USE_BADGE, wording::INSTALLED_IN_EDGE_BADGE]
            } else {
                vec![wording::INSTALLED_IN_EDGE_BADGE]
            };
            theme_row(&theme.id, &theme.name, badges)
        })
        .collect();
    let has_no_themes = own.is_empty() && edge.is_empty();
    ThemingView {
        labels: ThemingLabels {
            store_link_placeholder: wording::STORE_LINK_PLACEHOLDER.to_owned(),
            add: wording::ADD_THEME.to_owned(),
            add_local: wording::ADD_LOCAL_THEME.to_owned(),
            pick_file: wording::PICK_FILE.to_owned(),
            pick_folder: wording::PICK_FOLDER.to_owned(),
            when_light: wording::WHEN_LIGHT.to_owned(),
            when_dark: wording::WHEN_DARK.to_owned(),
            theme_list_name: wording::THEME_LIST_NAME.to_owned(),
            reorder_help: wording::REORDER_HELP.to_owned(),
        },
        options,
        light: state.theming.light.clone(),
        dark: state.theming.dark.clone(),
        own,
        edge,
        empty: has_no_themes.then(|| wording::NO_THEMES.to_owned()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_switch_key_names_one_card() {
        for slot in panel_layout::all_cards() {
            assert_eq!(Some(slot), slot_of(switch_key(slot)));
        }
    }
}
