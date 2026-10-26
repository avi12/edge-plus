//! Every sentence Edge+ shows: the window's (sent to it in each View), the
//! toast's and the errors'. The window itself holds none.

use crate::edge::version::edge_support::FeatureState;
use crate::edge::version::file_version::FileVersion;
use crate::platform::settings::Feature;
use crate::ui::panel_layout::{OwnCard, Section};
use crate::ui::panel_state::PanelState;

/// Must match the DLL's copy (`chat_target.h`).
const ASSISTANT_NAME: &str = "Copilot";
/// Must match the DLL's kRegExpName.
const REGEXP_NAME: &str = "RegExp";

/// The glyph is a Segoe Fluent Icons code point: a private-use character in a
/// string literal is invisible in an editor.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CardWording {
    pub title: String,
    pub description: String,
    pub glyph: u32,
}

fn card(title: impl Into<String>, description: impl Into<String>, glyph: u32) -> CardWording {
    CardWording {
        title: title.into(),
        description: description.into(),
        glyph,
    }
}

pub const OPEN_EDGE_HINT: &str = "Open Edge to apply changes.";

#[must_use]
pub fn section_name(section: Section) -> &'static str {
    match section {
        Section::TabsAndWindows => "Tabs and windows",
        Section::LookAndThemes => "Look and themes",
        Section::Browsing => "Browsing",
        Section::EdgePlusItself => "Edge+ itself",
    }
}

#[must_use]
pub fn needs_feature(needed: Feature) -> String {
    format!("Turn on {} to use this.", for_feature(needed).title)
}

/// The footer's "by Avi".
pub const BY: &str = "by";
pub const DROP_HINT: &str = "Drop a theme folder, .zip or .crx to add it";

pub const NOT_A_STORE_LINK: &str =
    "That is not a theme link from the Chrome Web Store or Edge Add-ons.";
pub const STORE_LINK_PLACEHOLDER: &str = "Paste a Chrome Web Store or Edge Add-ons link";
pub const ADD_THEME: &str = "Add";
pub const ADD_LOCAL_THEME: &str = "Or add a local theme";
pub const PICK_FILE: &str = "File…";
pub const PICK_FOLDER: &str = "Folder…";
pub const WHEN_LIGHT: &str = "When Windows is light";
pub const WHEN_DARK: &str = "When Windows is dark";
pub const DEFAULT_THEME: &str = "Default theme";
pub const STORE_BADGE: &str = "Store";
pub const LOCAL_BADGE: &str = "Local";
pub const IN_USE_BADGE: &str = "In use";
pub const INSTALLED_IN_EDGE_BADGE: &str = "Installed in Edge";
pub const NO_THEMES: &str =
    "No themes yet — paste a store link or add a local theme (.crx, .zip or folder).";

#[must_use]
pub fn remove_theme(name: &str) -> String {
    format!("Remove {name}")
}

pub const ACTION_FAILED: &str = "That did not work. Edge+ wrote the details to its log (panel.log in %LOCALAPPDATA%\\EdgePlus).";
pub const THEME_FILES_UNREADABLE: &str = "Edge+ could not read or write that theme's files. Check the folder still exists and is not open elsewhere.";
pub const MANIFEST_NOT_JSON: &str = "manifest.json is not valid JSON";
pub const IDENTITY_CHANGED: &str = "The theme's identity changed; add it again.";

// What is wrong with a local theme's files (theme_manifest, crx, protobuf).
pub const THEME_TOO_LARGE: &str = "the theme is too large";
pub const NOT_A_ZIP: &str = "not a valid ZIP";
pub const MANIFEST_NOT_AN_OBJECT: &str = "manifest.json: not an object";
pub const MANIFEST_HAS_NO_NAME: &str = "manifest.json has no name";
pub const MANIFEST_HAS_NO_VERSION: &str = "manifest.json has no version";
pub const MANIFEST_HAS_NO_THEME: &str = "manifest.json has no \"theme\" section";
pub const NOT_A_CRX3: &str = "not a CRX3 file";
pub const TRUNCATED_CRX_HEADER: &str = "truncated CRX header";
pub const TRUNCATED_VARINT: &str = "truncated varint";
pub const UNEXPECTED_CRX_HEADER_FIELD: &str = "unexpected CRX header field";
pub const NO_PUBLIC_KEY_IN_HEADER: &str = "no public key in CRX header";
pub const NO_PUBLIC_KEY_IN_PROOF: &str = "no public key in key proof";
pub const BAD_CRX_ZIP: &str = "bad .crx ZIP";
pub const NO_MANIFEST_IN_CRX: &str = "no manifest.json in .crx";
pub const BAD_CRX_MANIFEST: &str = "bad manifest.json";
pub const NO_VERSION_IN_CRX_MANIFEST: &str = "no version in manifest.json";

#[must_use]
pub fn manifest_not_json_at(line: usize) -> String {
    format!("{MANIFEST_NOT_JSON} (line {line})")
}

#[must_use]
pub fn entry_missing(name: &str) -> String {
    format!("{name} is missing")
}

#[must_use]
pub fn entry_too_large(name: &str) -> String {
    format!("{name} is too large")
}

#[must_use]
pub fn image_not_a_path(key: &str) -> String {
    format!("image {key} is not a path")
}

#[must_use]
pub fn image_unreadable(key: &str, path: &str, reason: &str) -> String {
    format!("image {key} ({path}): {reason}")
}

#[must_use]
pub fn image_not_png_or_jpeg(key: &str, path: &str) -> String {
    format!("image {key} ({path}) is not a PNG or JPEG")
}

#[must_use]
pub fn color_not_a_list(key: &str) -> String {
    format!("color {key} is not a list")
}

#[must_use]
pub fn color_wrong_count(key: &str) -> String {
    format!("color {key} needs 3 or 4 numbers")
}

#[must_use]
pub fn color_out_of_range(key: &str) -> String {
    format!("color {key} has a value out of range")
}

#[must_use]
pub fn tint_wrong_count(key: &str) -> String {
    format!("tint {key} needs 3 numbers")
}

#[must_use]
pub fn tint_out_of_range(key: &str) -> String {
    format!("tint {key} has a value out of range")
}

#[must_use]
pub fn not_a_valid_version(text: &str) -> String {
    format!("\"{text}\" is not a valid version")
}

pub const THEME_LIST_NAME: &str = "Your themes";
pub const REORDER_HELP: &str =
    "Drag a theme, or press Alt+Up or Alt+Down, to move it in this list and in the pickers.";

pub const INJECT_BUTTON: &str = "Inject into Edge";

#[must_use]
pub fn inject_ready() -> CardWording {
    card(
        "Edge is open without Edge+",
        "Inject automatically is off, so this Edge started without Edge+. Inject it to turn \
         your switches on.",
        0xE768,
    )
}

#[must_use]
pub fn injected() -> CardWording {
    card(
        "Edge+ is in Edge",
        "Injected. When Edge next starts, Edge+ offers this button again.",
        0xE73E,
    )
}

pub const UPDATE_BUTTON: &str = "Restart to update";
pub const UPDATE_RESTARTING: &str = "Restarting";
pub const UPDATE_FAILED: &str = "Edge+ could not replace itself where it is installed. Get the new version from its GitHub releases page instead.";

/// A copy the installer put under Program Files updates through the new
/// installer, which Windows asks to allow.
#[must_use]
pub fn update_ready(version: &str, is_installer: bool) -> CardWording {
    let how = if is_installer {
        "Restart Edge+ to finish updating, and allow the installer when Windows asks. "
    } else {
        "Restart Edge+ to finish updating. "
    };
    card(
        format!("Edge+ {version} is ready"),
        format!("{how}Edge keeps running, and your splits and themes stay as they are."),
        0xE777,
    )
}

#[must_use]
pub fn for_own(own: OwnCard) -> CardWording {
    match own {
        OwnCard::StartWithWindows => card(
            "Start Edge+ when Windows starts",
            "Runs Edge+ in the background, with no window, from when you sign in to Windows: \
             your settings apply and an Edge update that needs a newer Edge+ is caught. Launch \
             Edge+ to open its window.",
            0xE7E8,
        ),
        OwnCard::AutoInject => card(
            "Inject automatically",
            "Puts Edge+ into Edge each time Edge starts. Off, Edge starts without it and Edge+ \
             offers an Inject button.",
            0xE945,
        ),
        OwnCard::AutoUpdate => card(
            "Check for updates",
            "Looks for a new Edge+ release on GitHub and offers to restart into it. Off, Edge+ \
             never contacts GitHub.",
            0xE895,
        ),
    }
}

#[must_use]
pub fn for_feature(feature: Feature) -> CardWording {
    match feature {
        Feature::NativeSplit => card(
            "Split into any grid",
            "Grows Edge's own split past two panes: add panes on any side, drag the dividers, \
             move with Shift+Alt+arrow. Live, no restart. Open Edge first.",
            0xF0E2,
        ),
        Feature::Theming => card(
            "Theme by Windows light and dark",
            "Use one theme when Windows is light and another when it is dark, switched live. Add \
             themes from the Chrome Web Store, Edge Add-ons or a local file.",
            0xE790,
        ),
        Feature::WebUiFollowsTheme => card(
            "Settings and extensions pages follow the theme",
            "Makes edge://settings and edge://extensions light or dark to match your Edge theme, \
             not Windows. Only those two pages change; websites are untouched. Live, no \
             restart. Open Edge first.",
            0xE706,
        ),
        Feature::EnhancedFind => card(
            format!("Enhanced find: {REGEXP_NAME} and Ask {ASSISTANT_NAME}"),
            format!(
                "Adds {REGEXP_NAME} (with its flags) and Ask {ASSISTANT_NAME} to the options of \
                 Edge's Ctrl+F bar. {REGEXP_NAME} highlights every match; Ask sends your question \
                 to {ASSISTANT_NAME}, which opens in its side panel. Live, no restart. Open Edge \
                 first."
            ),
            0xE721,
        ),
        Feature::LivePreview => card(
            "Live tab preview on hover",
            "Hover a tab and its thumbnail plays live instead of a frozen snapshot, by keeping \
             Edge's own tab capture running while the preview is open. Live, no restart. Open \
             Edge first.",
            0xE890,
        ),
        Feature::TabShape => card(
            "Cut-corner tabs",
            "Gives every tab, vertical and horizontal, a straight leading edge and a cut trailing \
             corner, with the selected tab filled in the theme's neon. Works with Theming on. \
             Live, no restart.",
            0xE7C3,
        ),
        Feature::ReadingTheme => card(
            "Reading mode follows Windows light and dark",
            "Reading mode uses one page theme while Windows is light and another while it is \
             dark, switched as Windows switches. Pick both in reading mode: Appearance settings, \
             Themes, then Light mode or Dark mode. Live, no restart.",
            0xE736,
        ),
        Feature::QuietDevMode => card(
            "Hide the developer-mode prompt",
            "Stops Edge asking to turn off developer-mode extensions when it starts. Takes effect \
             from Edge's next start.",
            0xE7BA,
        ),
        Feature::ProtocolHandlers => card(
            "Ask to handle links, like Chrome",
            "Like Chrome: when a site such as Gmail offers to open email or calendar links, \
             closing the bubble no longer blocks it for good; only Ignore or Block does. Turning \
             this on lifts earlier blocks. Live, no restart.",
            0xE71B,
        ),
    }
}

#[must_use]
pub fn edge_lead(edge: FileVersion, supported: FileVersion) -> String {
    if edge > supported {
        format!("Edge updated to {edge}.")
    } else {
        format!("Edge is on version {edge}.")
    }
}

#[must_use]
pub fn describe_feature(state: &PanelState, feature: Feature) -> String {
    let verb = match state.state_of(feature) {
        FeatureState::Paused => "paused",
        FeatureState::Unavailable => "unavailable",
        FeatureState::On | FeatureState::Off => return String::new(),
    };
    let Some(edge) = state.edge_build else {
        return String::new();
    };
    format!(
        "{} {} is {verb} until Edge+ supports this version.",
        edge_lead(edge, state.supported_edge_build),
        for_feature(feature).title
    )
}

/// The window's notice and the toast's body.
#[must_use]
pub fn describe_paused(edge: FileVersion, supported: FileVersion, paused: &[Feature]) -> String {
    if paused.is_empty() {
        return String::new();
    }
    format!(
        "{} Paused until Edge+ supports this version: {}.",
        edge_lead(edge, supported),
        join_titles(paused)
    )
}

pub const PAUSED_TOAST_TITLE: &str = "Edge+ paused features for this Edge version";

/// Not commas: some titles carry their own ("Ask to handle links, like Chrome").
const TITLE_SEPARATOR: &str = "; ";

fn join_titles(features: &[Feature]) -> String {
    features
        .iter()
        .map(|&feature| for_feature(feature).title)
        .collect::<Vec<_>>()
        .join(TITLE_SEPARATOR)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::platform::settings::{Features, Theming};
    use crate::ui::panel_layout::{self, CardSlot};

    const SUPPORTED: FileVersion = FileVersion::new(155, 0, 4283, 45);
    const UPDATED: FileVersion = FileVersion::new(156, 0, 1, 0);

    fn state(choice: Features, edge: Option<FileVersion>) -> PanelState {
        PanelState {
            features: choice,
            theming: Theming::default(),
            edge_themes: Vec::new(),
            is_edge_running: edge.is_some(),
            is_start_with_windows: false,
            is_split_loaded: false,
            is_split_wanted: true,
            is_auto_inject: true,
            is_auto_update: true,
            edge_build: edge,
            supported_edge_build: SUPPORTED,
        }
    }

    #[test]
    fn every_card_has_words() {
        for feature in Feature::ALL {
            let wording = for_feature(feature);
            assert!(!wording.title.is_empty() && !wording.description.is_empty());
        }
        assert_eq!(
            "Enhanced find: RegExp and Ask Copilot",
            for_feature(Feature::EnhancedFind).title
        );
        assert!(
            for_feature(Feature::ReadingTheme)
                .description
                .contains("Appearance settings")
        );
    }

    #[test]
    fn a_held_card_says_why() {
        let mut choice = Features::none();
        choice.native_split = true;
        choice.theming = true;
        let updated = state(choice, Some(UPDATED));
        assert_eq!(
            "Edge updated to 156.0.1.0. Split into any grid is paused until Edge+ supports this \
             version.",
            describe_feature(&updated, Feature::NativeSplit)
        );
        assert_eq!(
            "Edge updated to 156.0.1.0. Live tab preview on hover is unavailable until Edge+ \
             supports this version.",
            describe_feature(&updated, Feature::LivePreview)
        );
        assert_eq!("", describe_feature(&updated, Feature::QuietDevMode));
        assert!(updated.is_locked(Feature::NativeSplit));
        assert!(!updated.is_locked(Feature::QuietDevMode));
        assert_eq!(
            "",
            describe_feature(&state(choice, Some(SUPPORTED)), Feature::NativeSplit)
        );
        assert_eq!(
            "Edge is on version 154.0.1.0.",
            edge_lead(FileVersion::new(154, 0, 1, 0), SUPPORTED)
        );
        assert_eq!(
            "Edge updated to 156.0.1.0. Paused until Edge+ supports this version: Split into any \
             grid; Theme by Windows light and dark.",
            describe_paused(UPDATED, SUPPORTED, &updated.paused())
        );
        assert_eq!("", describe_paused(UPDATED, SUPPORTED, &[]));
    }

    #[test]
    fn every_section_and_own_card_has_words() {
        for (section, _) in panel_layout::SECTIONS {
            assert_ne!("", section_name(section));
        }
        for slot in panel_layout::all_cards() {
            if let CardSlot::Own(own) = slot {
                assert_ne!("", for_own(own).title);
            }
        }
        assert_eq!(
            "Turn on Theme by Windows light and dark to use this.",
            needs_feature(Feature::Theming)
        );
    }
}
