//! Edge's profile folder and the JSON files in it.

use std::path::{Path, PathBuf};

pub const PREFERENCES_FILE: &str = "Preferences";
pub const LOCAL_STATE_FILE: &str = "Local State";

/// %LOCALAPPDATA%\Microsoft\Edge\User Data
#[must_use]
pub fn default_user_data() -> PathBuf {
    todo!()
}

/// Edge writes some of these with a UTF-8 byte-order mark. None when missing or
/// not JSON.
#[must_use]
pub fn read_json(path: &Path) -> Option<serde_json::Value> {
    let _ = path;
    todo!()
}
