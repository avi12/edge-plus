//! Which theme Edge wears and who put it there, as the DLL last reported it.
//! Mirrors struct `Wire` in `app/dll/src/core/wear_report.h` field for field, every
//! u32 little-endian, then the id's bytes.
//!
//! The DLL writes it from inside Edge the moment Edge changes theme, so it is
//! exact where Edge's Preferences file is late (Edge writes that one when it
//! likes, and records no theme in the middle of a switch).

use std::fs;

use crate::platform::{paths, shared};

const FIELD_BYTES: usize = size_of::<u32>();
const FIELDS_BYTES: usize = shared::WEAR_FIELD_COUNT as usize * FIELD_BYTES;
const WIRE_BYTES: usize = FIELDS_BYTES + shared::WIRE_ID_CAPACITY as usize;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WearCause {
    EdgePlus,
    /// Inside Edge: its "Restore default", or a theme from a store.
    User,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Wear {
    /// Differs between two reports.
    pub stamp: u32,
    pub cause: WearCause,
    /// Empty for Edge's default theme.
    pub theme_id: String,
}

impl Wear {
    /// None for a file of the wrong size, a cause this panel does not know or
    /// an id that is not UTF-8.
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
        let cause = match field(1)? {
            shared::WEAR_CAUSE_EDGE_PLUS => WearCause::EdgePlus,
            shared::WEAR_CAUSE_USER => WearCause::User,
            _ => return None,
        };
        let id_length = usize::try_from(field(2)?).ok()?;
        let id_bytes = bytes.get(FIELDS_BYTES..FIELDS_BYTES + id_length)?;
        Some(Self {
            stamp: field(0)?,
            cause,
            theme_id: String::from_utf8(id_bytes.to_vec()).ok()?,
        })
    }

    #[must_use]
    pub fn is_users_default(&self) -> bool {
        self.cause == WearCause::User && self.theme_id.is_empty()
    }
}

/// The DLL's last report; None while none was written on this machine.
#[must_use]
pub fn read() -> Option<Wear> {
    Wear::decode(&fs::read(paths::under_local_app_data(shared::WEAR_FILE_RELATIVE_PATH)).ok()?)
}

#[cfg(test)]
mod tests {
    use super::*;

    const THEME: &str = "abcdefghijklmnopabcdefghijklmnop";

    fn wire(stamp: u32, cause: u32, theme_id: &str) -> Vec<u8> {
        let length = u32::try_from(theme_id.len()).expect("a short id");
        let mut bytes = Vec::new();
        for field in [stamp, cause, length] {
            bytes.extend_from_slice(&field.to_le_bytes());
        }
        bytes.extend_from_slice(theme_id.as_bytes());
        bytes.resize(WIRE_BYTES, 0);
        bytes
    }

    #[test]
    fn reads_a_theme_and_who_put_it_on() {
        let wear = Wear::decode(&wire(7, shared::WEAR_CAUSE_EDGE_PLUS, THEME)).expect("a wear");
        assert_eq!(
            wear,
            Wear {
                stamp: 7,
                cause: WearCause::EdgePlus,
                theme_id: THEME.to_owned(),
            }
        );
        assert!(!wear.is_users_default());
    }

    #[test]
    fn only_the_users_own_default_is_followed() {
        let users = Wear::decode(&wire(1, shared::WEAR_CAUSE_USER, "")).expect("a wear");
        let own = Wear::decode(&wire(1, shared::WEAR_CAUSE_EDGE_PLUS, "")).expect("a wear");
        let users_theme = Wear::decode(&wire(1, shared::WEAR_CAUSE_USER, THEME)).expect("a wear");
        assert!(users.is_users_default());
        assert!(!own.is_users_default());
        assert!(!users_theme.is_users_default());
    }

    #[test]
    fn refuses_a_wrong_size_an_unknown_cause_or_an_overlong_id() {
        let mut short = wire(1, shared::WEAR_CAUSE_USER, "");
        short.pop();
        assert_eq!(None, Wear::decode(&short));
        assert_eq!(None, Wear::decode(&wire(1, 9, "")));
        let mut overlong = wire(1, shared::WEAR_CAUSE_USER, "");
        overlong[8..12].copy_from_slice(&(shared::WIRE_ID_CAPACITY + 1).to_le_bytes());
        assert_eq!(None, Wear::decode(&overlong));
    }
}
