//! The order of settings.json's themes array is the order of the list and of
//! the light / dark pickers.

use crate::platform::settings::ThemeEntry;

/// A theme `ids` does not name goes after the named ones (it was added while
/// the user was dragging); an id with no theme was removed meanwhile.
#[must_use]
pub fn ordered(themes: &[ThemeEntry], ids: &[String]) -> Vec<ThemeEntry> {
    let _ = (themes, ids);
    todo!()
}
