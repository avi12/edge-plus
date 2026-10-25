//! Read-only, straight from Edge's profiles on disk: the DLL applies an
//! installed theme by id exactly like one the panel installed.

use std::path::Path;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct InstalledTheme {
    pub id: String,
    pub name: String,
    pub is_active: bool,
}

/// Each id once, active if it is active in any profile, ordered by name.
#[must_use]
pub fn scan(user_data: &Path) -> Vec<InstalledTheme> {
    let _ = user_data;
    todo!()
}
