//! Mirrors struct Wire in app/native-split/src/config.h field for field, every
//! u32 little-endian as the DLL reads it on x64. The reserved u32 was once the
//! chosen assistant: written as zero, never read.

use crate::platform::settings::{Features, Settings};
use crate::platform::shared;

/// A theme id is an extension id, or empty for the default theme.
#[derive(Clone, Debug, PartialEq, Eq)]
#[allow(
    clippy::struct_excessive_bools,
    reason = "each mirrors one flag of the wire"
)]
pub struct DllConfig {
    pub is_split_active: bool,
    pub is_theming_active: bool,
    pub is_protocol_handlers_active: bool,
    pub light_id: String,
    pub dark_id: String,
    pub is_enhanced_find_active: bool,
    pub is_web_ui_follows_theme_active: bool,
    pub is_live_preview_active: bool,
    pub is_tab_shape_active: bool,
    pub is_reading_theme_active: bool,
}

impl DllConfig {
    /// `features` as they apply on the current Edge build (a paused feature
    /// off).
    #[must_use]
    pub fn from_features(settings: &Settings, features: &Features) -> Self {
        Self {
            is_split_active: features.native_split,
            is_theming_active: features.theming,
            is_protocol_handlers_active: features.protocol_handlers,
            light_id: settings.theming.light.clone(),
            dark_id: settings.theming.dark.clone(),
            is_enhanced_find_active: features.enhanced_find,
            is_web_ui_follows_theme_active: features.web_ui_follows_theme,
            is_live_preview_active: features.live_preview,
            is_tab_shape_active: features.tab_shape,
            is_reading_theme_active: features.reading_theme,
        }
    }
}

const FIELD_BYTES: usize = size_of::<u32>();
const ID_CAPACITY: usize = shared::WIRE_ID_CAPACITY as usize;

pub const SPLIT_ACTIVE_OFFSET: usize = 0;
pub const THEMING_ACTIVE_OFFSET: usize = SPLIT_ACTIVE_OFFSET + FIELD_BYTES;
pub const PROTOCOL_HANDLERS_ACTIVE_OFFSET: usize = THEMING_ACTIVE_OFFSET + FIELD_BYTES;
pub const LIGHT_LENGTH_OFFSET: usize = PROTOCOL_HANDLERS_ACTIVE_OFFSET + FIELD_BYTES;
pub const DARK_LENGTH_OFFSET: usize = LIGHT_LENGTH_OFFSET + FIELD_BYTES;
pub const LIGHT_ID_OFFSET: usize = DARK_LENGTH_OFFSET + FIELD_BYTES;
pub const DARK_ID_OFFSET: usize = LIGHT_ID_OFFSET + ID_CAPACITY;
pub const RESERVED_OFFSET: usize = DARK_ID_OFFSET + ID_CAPACITY;
pub const ENHANCED_FIND_ACTIVE_OFFSET: usize = RESERVED_OFFSET + FIELD_BYTES;
pub const WEB_UI_FOLLOWS_THEME_ACTIVE_OFFSET: usize = ENHANCED_FIND_ACTIVE_OFFSET + FIELD_BYTES;
pub const LIVE_PREVIEW_ACTIVE_OFFSET: usize = WEB_UI_FOLLOWS_THEME_ACTIVE_OFFSET + FIELD_BYTES;
pub const TAB_SHAPE_ACTIVE_OFFSET: usize = LIVE_PREVIEW_ACTIVE_OFFSET + FIELD_BYTES;
pub const READING_THEME_ACTIVE_OFFSET: usize = TAB_SHAPE_ACTIVE_OFFSET + FIELD_BYTES;
pub const SIZE: usize = READING_THEME_ACTIVE_OFFSET + FIELD_BYTES;

/// What config.h asserts at compile time, from its own counts.
pub const SIZE_CONFIG_H_EXPECTS: usize =
    shared::WIRE_FIELD_COUNT as usize * FIELD_BYTES + shared::WIRE_ID_COUNT as usize * ID_CAPACITY;

/// A field added on one side only fails the build, not Edge.
const _: () = assert!(
    SIZE == SIZE_CONFIG_H_EXPECTS,
    "the wire must match config.h"
);

#[must_use]
pub fn encode(config: &DllConfig) -> [u8; SIZE] {
    let mut wire = [0_u8; SIZE];
    write_flag(&mut wire, SPLIT_ACTIVE_OFFSET, config.is_split_active);
    write_flag(&mut wire, THEMING_ACTIVE_OFFSET, config.is_theming_active);
    write_flag(
        &mut wire,
        PROTOCOL_HANDLERS_ACTIVE_OFFSET,
        config.is_protocol_handlers_active,
    );
    write_id(
        &mut wire,
        LIGHT_LENGTH_OFFSET,
        LIGHT_ID_OFFSET,
        &config.light_id,
    );
    write_id(
        &mut wire,
        DARK_LENGTH_OFFSET,
        DARK_ID_OFFSET,
        &config.dark_id,
    );
    write_flag(
        &mut wire,
        ENHANCED_FIND_ACTIVE_OFFSET,
        config.is_enhanced_find_active,
    );
    write_flag(
        &mut wire,
        WEB_UI_FOLLOWS_THEME_ACTIVE_OFFSET,
        config.is_web_ui_follows_theme_active,
    );
    write_flag(
        &mut wire,
        LIVE_PREVIEW_ACTIVE_OFFSET,
        config.is_live_preview_active,
    );
    write_flag(
        &mut wire,
        TAB_SHAPE_ACTIVE_OFFSET,
        config.is_tab_shape_active,
    );
    write_flag(
        &mut wire,
        READING_THEME_ACTIVE_OFFSET,
        config.is_reading_theme_active,
    );
    wire
}

fn write_u32(wire: &mut [u8], offset: usize, value: u32) {
    wire[offset..offset + FIELD_BYTES].copy_from_slice(&value.to_le_bytes());
}

fn write_flag(wire: &mut [u8], offset: usize, is_on: bool) {
    write_u32(wire, offset, u32::from(is_on));
}

/// An id longer than its field is cut to it, never spilling into the next.
fn write_id(wire: &mut [u8], length_offset: usize, id_offset: usize, id: &str) {
    let bytes = id.as_bytes();
    let length = bytes.len().min(ID_CAPACITY);
    let length_field = u32::try_from(length).expect("an id's field is far below 4 GB");
    write_u32(wire, length_offset, length_field);
    wire[id_offset..id_offset + length].copy_from_slice(&bytes[..length]);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::platform::settings::ColorMode;

    fn from_settings(settings: &Settings) -> DllConfig {
        DllConfig::from_features(settings, &settings.features)
    }

    fn u32_at(wire: &[u8], offset: usize) -> u32 {
        let mut field = [0_u8; FIELD_BYTES];
        field.copy_from_slice(&wire[offset..offset + FIELD_BYTES]);
        u32::from_le_bytes(field)
    }

    fn config(light_id: &str, is_on: bool) -> DllConfig {
        DllConfig {
            is_split_active: false,
            is_theming_active: is_on,
            is_protocol_handlers_active: is_on,
            light_id: light_id.to_owned(),
            dark_id: String::new(),
            is_enhanced_find_active: is_on,
            is_web_ui_follows_theme_active: is_on,
            is_live_preview_active: is_on,
            is_tab_shape_active: is_on,
            is_reading_theme_active: is_on,
        }
    }

    #[test]
    fn the_wire_is_config_hs_172_bytes() {
        assert_eq!(172, SIZE);
        assert_eq!(
            SIZE_CONFIG_H_EXPECTS, SIZE,
            "the layout matches config.h's own counts"
        );
    }

    #[test]
    fn every_field_lands_at_its_offset() {
        let wire = encode(&config("abc", true));
        assert_eq!(164, TAB_SHAPE_ACTIVE_OFFSET, "the tab shape at 164");
        assert_eq!(1, u32_at(&wire, 164), "tab shape on");
        assert_eq!(
            168, READING_THEME_ACTIVE_OFFSET,
            "the reading theme switch at 168"
        );
        assert_eq!(1, u32_at(&wire, 168), "reading theme on");
        let defaults = encode(&from_settings(&Settings::default()));
        assert_eq!(0, u32_at(&defaults, 168), "reading mode is off by default");
        assert_eq!(0, u32_at(&wire, 0), "split off at 0");
        assert_eq!(1, u32_at(&wire, 4), "theming on at 4");
        assert_eq!(1, u32_at(&wire, 8), "link handlers on at 8");
        assert_eq!(3, u32_at(&wire, 12), "the light id's length at 12");
        assert_eq!(0, u32_at(&wire, 16), "no dark id at 16");
        assert_eq!(b"abc", &wire[20..23], "the light id at 20");
        assert_eq!(
            0,
            u32_at(&wire, RESERVED_OFFSET),
            "the reserved field is zero"
        );
        assert_eq!(152, ENHANCED_FIND_ACTIVE_OFFSET, "find at 152");
        assert_eq!(156, WEB_UI_FOLLOWS_THEME_ACTIVE_OFFSET, "WebUI at 156");
        assert_eq!(160, LIVE_PREVIEW_ACTIVE_OFFSET, "live preview at 160");
        assert_eq!(1, u32_at(&wire, 152), "find on");
        assert_eq!(1, u32_at(&wire, 156), "WebUI on");
        assert_eq!(1, u32_at(&wire, 160), "live preview on");
    }

    #[test]
    fn an_id_is_cut_to_its_field() {
        let too_long = encode(&config(&"x".repeat(100), false));
        assert_eq!(shared::WIRE_ID_CAPACITY, u32_at(&too_long, 12));
        assert_eq!(
            0, too_long[DARK_ID_OFFSET],
            "a cut id does not spill into the next"
        );
    }

    #[test]
    fn the_config_follows_the_settings() {
        let mut settings = Settings::default();
        settings.theming.assign(ColorMode::Dark, "dark-id");
        let from_settings = from_settings(&settings);
        assert!(
            from_settings.is_split_active && !from_settings.is_theming_active,
            "the config follows the switches"
        );
        assert_eq!(
            "dark-id", from_settings.dark_id,
            "the config follows the assignment"
        );
    }
}
