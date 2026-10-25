//! Edge's profile folder and the JSON files in it.

use std::fs;
use std::path::{Path, PathBuf};

use crate::platform::paths;

pub const PREFERENCES_FILE: &str = "Preferences";
pub const LOCAL_STATE_FILE: &str = "Local State";
const BYTE_ORDER_MARK: char = '\u{feff}';

/// %LOCALAPPDATA%\Microsoft\Edge\User Data
#[must_use]
pub fn default_user_data() -> PathBuf {
    paths::local_app_data()
        .join("Microsoft")
        .join("Edge")
        .join("User Data")
}

/// Edge writes some of these with a UTF-8 byte-order mark. None when missing or
/// not JSON.
#[must_use]
pub fn read_json(path: &Path) -> Option<serde_json::Value> {
    let text = fs::read_to_string(path).ok()?;
    serde_json::from_str(text.trim_start_matches(BYTE_ORDER_MARK)).ok()
}
