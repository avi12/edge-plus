//! What the DLL says about the Edge it loaded into. Mirrors struct Wire in
//! app/native-split/src/core/status.h field for field, every u32 little-endian.
//!
//! On a build offsets.h is not pinned to, the DLL looks for the code it hooks by
//! signature and reports whether it could (and which features that leaves); the
//! panel pauses a feature only on that word, not on the version number.

use std::fs;
use std::io;

use crate::edge::version::file_version::FileVersion;
use crate::platform::settings::Feature;
use crate::platform::{paths, shared};

const FIELD_BYTES: usize = size_of::<u32>();
const WIRE_BYTES: usize = shared::STATUS_FIELD_COUNT as usize * FIELD_BYTES;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    /// The build offsets.h is pinned to.
    Pinned,
    /// Another build; the hooks were found by signature.
    Resolved,
    /// Another build; nothing was safe to hook, so the DLL left.
    Refused,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DllStatus {
    pub mode: Mode,
    pub build: FileVersion,
    /// The `FEATURE_BIT_*` of every feature that can run in that build.
    pub available_features: u32,
    pub resolved_entries: u32,
    pub total_entries: u32,
}

impl DllStatus {
    /// None for a file of the wrong size or a mode this panel does not know.
    #[must_use]
    pub fn decode(bytes: &[u8]) -> Option<Self> {
        if bytes.len() != WIRE_BYTES {
            return None;
        }
        let field = |index: usize| {
            let start = index * FIELD_BYTES;
            let raw: [u8; FIELD_BYTES] = bytes[start..start + FIELD_BYTES].try_into().ok()?;
            Some(u32::from_le_bytes(raw))
        };
        let mode = match field(0)? {
            shared::STATUS_MODE_PINNED => Mode::Pinned,
            shared::STATUS_MODE_RESOLVED => Mode::Resolved,
            shared::STATUS_MODE_REFUSED => Mode::Refused,
            _ => return None,
        };
        let part = |index: usize| u16::try_from(field(index)?).ok();
        Some(Self {
            mode,
            build: FileVersion::new(part(1)?, part(2)?, part(3)?, part(4)?),
            available_features: field(5)?,
            resolved_entries: field(6)?,
            total_entries: field(7)?,
        })
    }
}

/// The bit the DLL gives a feature (`signature_table.h`); None for a feature that
/// does not go through the DLL.
#[must_use]
pub fn feature_bit(feature: Feature) -> Option<u32> {
    match feature {
        Feature::NativeSplit => Some(shared::FEATURE_BIT_SPLIT),
        Feature::Theming => Some(shared::FEATURE_BIT_THEMING),
        Feature::ProtocolHandlers => Some(shared::FEATURE_BIT_PROTOCOL_HANDLERS),
        Feature::EnhancedFind => Some(shared::FEATURE_BIT_ENHANCED_FIND),
        Feature::WebUiFollowsTheme => Some(shared::FEATURE_BIT_WEB_UI_FOLLOWS_THEME),
        Feature::LivePreview => Some(shared::FEATURE_BIT_LIVE_PREVIEW),
        Feature::CyberpunkSkin => Some(shared::FEATURE_BIT_CYBERPUNK_SKIN),
        Feature::ReadingTheme => Some(shared::FEATURE_BIT_READING_THEME),
        Feature::QuietDevMode => None,
    }
}

fn status_file() -> std::path::PathBuf {
    let mut path = paths::local_app_data().into_os_string();
    path.push(shared::STATUS_FILE_RELATIVE_PATH);
    path.into()
}

/// The DLL's last word; None while no DLL has reported on this machine.
#[must_use]
pub fn read() -> Option<DllStatus> {
    DllStatus::decode(&fs::read(status_file()).ok()?)
}

/// A word from a DLL that is no longer the one carried is not worth keeping:
/// the next one may place what the last could not.
pub fn forget() -> io::Result<()> {
    match fs::remove_file(status_file()) {
        Err(error) if error.kind() != io::ErrorKind::NotFound => Err(error),
        _ => Ok(()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn wire(fields: [u32; 8]) -> Vec<u8> {
        fields
            .iter()
            .flat_map(|field| field.to_le_bytes())
            .collect()
    }

    #[test]
    fn a_resolved_build_names_its_features() {
        let bytes = wire([
            shared::STATUS_MODE_RESOLVED,
            157,
            0,
            4400,
            12,
            shared::FEATURE_BIT_SPLIT | shared::FEATURE_BIT_THEMING,
            280,
            284,
        ]);
        let status = DllStatus::decode(&bytes).expect("decodes");
        assert_eq!(Mode::Resolved, status.mode);
        assert_eq!(FileVersion::new(157, 0, 4400, 12), status.build);
        assert_eq!(
            shared::FEATURE_BIT_SPLIT | shared::FEATURE_BIT_THEMING,
            status.available_features
        );
        assert_eq!(None, feature_bit(Feature::QuietDevMode));
        assert_eq!((280, 284), (status.resolved_entries, status.total_entries));
    }

    #[test]
    fn a_refusal_decodes_and_nothing_else_does() {
        let refused = wire([shared::STATUS_MODE_REFUSED, 157, 0, 4400, 12, 0, 10, 284]);
        assert_eq!(
            Mode::Refused,
            DllStatus::decode(&refused).expect("decodes").mode
        );
        assert_eq!(None, DllStatus::decode(&refused[..refused.len() - 1]));
        assert_eq!(
            None,
            DllStatus::decode(&wire([9, 157, 0, 4400, 12, 0, 0, 0]))
        );
        assert_eq!(
            None,
            DllStatus::decode(&wire([0, 99_999, 0, 4400, 12, 0, 0, 0])),
            "a version part past 16 bits is not a build"
        );
    }

    #[test]
    fn every_dll_feature_has_a_distinct_bit() {
        let bits: Vec<u32> = Feature::ALL.into_iter().filter_map(feature_bit).collect();
        assert_eq!(8, bits.len());
        for (index, bit) in bits.iter().enumerate() {
            assert!(bit.is_power_of_two());
            assert!(!bits[..index].contains(bit));
        }
    }
}
