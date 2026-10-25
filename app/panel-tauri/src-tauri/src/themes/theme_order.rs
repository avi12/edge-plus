//! The order of settings.json's themes array is the order of the list and of
//! the light / dark pickers.

use crate::platform::settings::ThemeEntry;

/// Negative `offset` is up; stops at either end.
#[must_use]
pub fn moved(ids: &[String], id: &str, offset: i32) -> Vec<String> {
    let _ = (ids, id, offset);
    todo!()
}

/// A theme `ids` does not name goes after the named ones (it was added while
/// the user was dragging); an id with no theme was removed meanwhile.
#[must_use]
pub fn ordered(themes: &[ThemeEntry], ids: &[String]) -> Vec<ThemeEntry> {
    let _ = (themes, ids);
    todo!()
}
