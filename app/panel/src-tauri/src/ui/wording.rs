//! The sentences the core itself says: the paused toast's, and the errors an
//! action answers with. The window's own text is in its markup (`ui/window`);
//! the feature titles and the paused sentence, which both say, are in
//! `ui/window/wording/shared-wording.json`, read here at build time.

use std::collections::HashMap;
use std::sync::LazyLock;

use serde::Deserialize;

use crate::platform::settings::{ColorMode, Feature};
use crate::ui::protocol::EdgeBuild;

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
struct SharedWording {
    feature_titles: HashMap<Feature, String>,
    edge_updated: String,
    edge_on_version: String,
    paused_until_supported: String,
    /// Not commas: some titles carry their own ("Ask to handle links, like
    /// Chrome").
    title_separator: String,
}

static SHARED: LazyLock<SharedWording> = LazyLock::new(|| {
    serde_json::from_str(include_str!(
        "../../../ui/window/wording/shared-wording.json"
    ))
    .expect("shared-wording.json is checked by its test")
});

const EDGE_PLACEHOLDER: &str = "{edge}";
const LEAD_PLACEHOLDER: &str = "{lead}";
const TITLES_PLACEHOLDER: &str = "{titles}";

pub const NOT_A_STORE_LINK: &str =
    "That is not a theme link from the Chrome Web Store or Edge Add-ons.";
pub const EDGE_THEME_NOT_REMOVABLE: &str =
    "Edge+ cannot remove this theme because Edge installed it from its own store.";
pub const ACTION_FAILED: &str = "That did not work. Edge+ wrote the details to its log (panel.log in %LOCALAPPDATA%\\EdgePlus).";
pub const THEME_FILES_UNREADABLE: &str = "Edge+ could not read or write that theme's files. Check the folder still exists and is not open elsewhere.";
pub const MANIFEST_NOT_JSON: &str = "manifest.json is not valid JSON";
pub const IDENTITY_CHANGED: &str = "The theme's identity changed; add it again.";
pub const UPDATE_FAILED: &str = "Edge+ could not replace itself where it is installed. Get the new version from its GitHub releases page instead.";

// What is wrong with a local theme's files (theme_manifest, crx, protobuf).
pub const THEME_TOO_LARGE: &str = "the theme is too large";
pub const NOT_A_ZIP: &str = "not a valid ZIP";
pub const MANIFEST_NOT_AN_OBJECT: &str = "manifest.json: not an object";
pub const MANIFEST_HAS_NO_NAME: &str = "manifest.json has no name";
pub const MANIFEST_HAS_NO_VERSION: &str = "manifest.json has no version";
pub const COLOR_NEEDS_CHANNELS: &str = "needs 3 or 4 numbers";
pub const TINT_NEEDS_PARTS: &str = "needs 3 numbers";
pub const VALUE_OUT_OF_RANGE: &str = "has a value out of range";
pub const MANIFEST_VERSION_NOT_SUPPORTED: &str = "must be 2 or 3";
pub const MANIFEST_HAS_NO_MANIFEST_VERSION: &str = "manifest.json has no manifest_version";
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
pub fn theme_files_gone(name: &str) -> String {
    format!("The files of {name} are gone, so Edge uses its default theme.")
}

#[must_use]
pub fn edge_went_default(mode: ColorMode) -> String {
    let mode_name = match mode {
        ColorMode::Light => "light",
        ColorMode::Dark => "dark",
    };
    format!(
        "Edge went back to its default theme, so Edge+ shows the default theme for {mode_name} mode."
    )
}

pub fn manifest_not_json_at(line: usize) -> String {
    format!("{MANIFEST_NOT_JSON} (line {line})")
}

#[must_use]
pub fn edge_rejected_theme(reason: &str) -> String {
    format!("Edge refused the theme: {reason}")
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
pub fn image_unreadable(key: &str, path: &str, reason: &str) -> String {
    format!("image {key} ({path}): {reason}")
}

#[must_use]
pub fn image_not_png_or_jpeg(key: &str, path: &str) -> String {
    format!("image {key} ({path}) is not a PNG or JPEG")
}

#[must_use]
pub fn not_a_valid_version(text: &str) -> String {
    format!("\"{text}\" is not a valid version")
}

pub const PAUSED_TOAST_TITLE: &str = "Edge+ paused features for this Edge version";

#[must_use]
pub fn feature_title(feature: Feature) -> &'static str {
    SHARED
        .feature_titles
        .get(&feature)
        .map_or("", String::as_str)
}

/// The toast's body; the window says the same from the same file.
#[must_use]
pub fn describe_paused(edge: &EdgeBuild, paused: &[Feature]) -> String {
    if paused.is_empty() {
        return String::new();
    }
    let lead_template = if edge.is_updated {
        &SHARED.edge_updated
    } else {
        &SHARED.edge_on_version
    };
    let titles = paused
        .iter()
        .map(|&feature| feature_title(feature))
        .collect::<Vec<_>>()
        .join(&SHARED.title_separator);
    SHARED
        .paused_until_supported
        .replace(
            LEAD_PLACEHOLDER,
            &lead_template.replace(EDGE_PLACEHOLDER, &edge.version),
        )
        .replace(TITLES_PLACEHOLDER, &titles)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::edge::version::file_version::FileVersion;

    const SUPPORTED: FileVersion = FileVersion::new(155, 0, 4283, 45);
    const UPDATED: FileVersion = FileVersion::new(156, 0, 1, 0);

    #[test]
    fn every_feature_has_a_title() {
        for feature in Feature::ALL {
            assert_ne!("", feature_title(feature), "{feature:?}");
        }
        assert_eq!(
            "Enhanced find: RegExp and Ask Copilot",
            feature_title(Feature::EnhancedFind)
        );
    }

    #[test]
    fn the_paused_sentence_names_the_build_and_the_features() {
        assert_eq!(
            "Edge updated to 156.0.1.0. Paused until Edge+ supports this version: Split into any \
             grid; Theme by Windows light and dark.",
            describe_paused(
                &EdgeBuild::of(UPDATED, SUPPORTED),
                &[Feature::NativeSplit, Feature::Theming]
            )
        );
        assert_eq!(
            "Edge is on version 154.0.1.0. Paused until Edge+ supports this version: Cyberpunk \
             skin.",
            describe_paused(
                &EdgeBuild::of(FileVersion::new(154, 0, 1, 0), SUPPORTED),
                &[Feature::CyberpunkSkin]
            )
        );
        assert_eq!("", describe_paused(&EdgeBuild::of(UPDATED, SUPPORTED), &[]));
    }
}
