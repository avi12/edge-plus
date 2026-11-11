//! The shape of an extension manifest as a theme uses it, written as a schema:
//! `Manifest` is `z.object({ name, version, theme })`, and the leaves check
//! themselves (`Version`, `Color`, `Tint`). Parse it with `schema::parse`.

use std::collections::BTreeMap;

use serde::{Deserialize, Deserializer};
use serde_json::Number;

use crate::themes::package::schema::refuse;
use crate::ui::wording;

const SUPPORTED_MANIFEST_VERSIONS: [u64; 2] = [2, 3];
const VERSION_SEPARATOR: char = '.';
const MAX_VERSION_PARTS: usize = 4;
const COLOR_CHANNEL_MAX: i64 = 255;
const RGB_CHANNELS: usize = 3;
const RGBA_CHANNELS: usize = 4;
const ALPHA_INDEX: usize = 3;
const TINT_PARTS: usize = 3;
const TINT_UNCHANGED: f64 = -1.0;

/// Every field is optional here so that a missing one is worded for the user
/// by whoever asks for it, not by the parser.
#[derive(Debug, Deserialize)]
pub struct Manifest {
    pub name: Option<String>,
    #[serde(rename = "manifest_version")]
    pub format: Option<ManifestVersion>,
    pub version: Option<Version>,
    pub theme: Option<Theme>,
}

/// `images` name files in the archive, `colors` and `tints` are Chromium's.
#[derive(Debug, Deserialize)]
pub struct Theme {
    pub images: Option<BTreeMap<String, String>>,
    #[expect(
        clippy::zero_sized_map_values,
        dead_code,
        reason = "a color is checked as it is read; only that it passed is kept"
    )]
    pub colors: Option<BTreeMap<String, Color>>,
    #[expect(
        clippy::zero_sized_map_values,
        dead_code,
        reason = "a tint is checked as it is read; only that it passed is kept"
    )]
    pub tints: Option<BTreeMap<String, Tint>>,
}

/// Edge's loader refuses a manifest without `manifest_version` 2 or 3.
#[derive(Debug)]
pub struct ManifestVersion;

impl<'de> Deserialize<'de> for ManifestVersion {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let number = u64::deserialize(deserializer).ok();
        let is_supported =
            number.is_some_and(|number| SUPPORTED_MANIFEST_VERSIONS.contains(&number));
        if is_supported {
            return Ok(Self);
        }
        Err(refuse::<D>(wording::MANIFEST_VERSION_NOT_SUPPORTED))
    }
}

/// Up to four numeric parts, `1.2.3.4`.
#[derive(Debug)]
pub struct Version(pub Vec<u16>);

impl<'de> Deserialize<'de> for Version {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let text = String::deserialize(deserializer)?;
        let pieces: Vec<&str> = text.split(VERSION_SEPARATOR).collect();
        let parts = pieces
            .iter()
            .map(|piece| digits(piece))
            .collect::<Option<Vec<u16>>>()
            .filter(|_| pieces.len() <= MAX_VERSION_PARTS);
        match parts {
            Some(parts) => Ok(Self(parts)),
            None => Err(refuse::<D>(wording::not_a_valid_version(&text))),
        }
    }
}

/// Digits only, as a version part or a revision count is written: no sign, no
/// space.
#[must_use]
pub fn digits(text: &str) -> Option<u16> {
    let is_digits = !text.is_empty() && text.bytes().all(|byte| byte.is_ascii_digit());
    if !is_digits {
        return None;
    }
    text.parse().ok()
}

/// Red, green and blue as 0 to 255, and optionally an alpha as 0 to 1; held only as proof it was checked.
#[derive(Debug)]
pub struct Color;

impl<'de> Deserialize<'de> for Color {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let channels = Vec::<Number>::deserialize(deserializer)
            .map_err(|_| refuse::<D>(wording::COLOR_NEEDS_CHANNELS))?;
        let is_rgb = channels.len() == RGB_CHANNELS;
        let is_rgba = channels.len() == RGBA_CHANNELS;
        if !is_rgb && !is_rgba {
            return Err(refuse::<D>(wording::COLOR_NEEDS_CHANNELS));
        }
        let is_valid = channels.iter().enumerate().all(|(index, channel)| {
            let is_alpha = index == ALPHA_INDEX;
            if is_alpha {
                return channel
                    .as_f64()
                    .is_some_and(|alpha| (0.0..=1.0).contains(&alpha));
            }
            // A fraction is not a channel: 255.0 has no integer value here.
            channel
                .as_i64()
                .is_some_and(|byte| (0..=COLOR_CHANNEL_MAX).contains(&byte))
        });
        if !is_valid {
            return Err(refuse::<D>(wording::VALUE_OUT_OF_RANGE));
        }
        Ok(Self)
    }
}

/// Hue, saturation and lightness shifts, each 0 to 1 or -1 for unchanged; held only as proof it was checked.
#[derive(Debug)]
pub struct Tint;

impl<'de> Deserialize<'de> for Tint {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let parts = Vec::<f64>::deserialize(deserializer)
            .ok()
            .filter(|parts| parts.len() == TINT_PARTS)
            .ok_or_else(|| refuse::<D>(wording::TINT_NEEDS_PARTS))?;
        let is_valid = parts.iter().all(|number| {
            let is_unchanged = (number - TINT_UNCHANGED).abs() < f64::EPSILON;
            is_unchanged || (0.0..=1.0).contains(number)
        });
        if !is_valid {
            return Err(refuse::<D>(wording::VALUE_OUT_OF_RANGE));
        }
        Ok(Self)
    }
}
