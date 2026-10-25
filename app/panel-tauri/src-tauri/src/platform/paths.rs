//! The data folder is a contract with the DLL (config.h, log.h), so its
//! location comes from the DLL's own constant (shared). Earlier panels kept it
//! in a folder of the project's old name; legacy_install carries that over.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use crate::platform::shared;

const SETTINGS_FILE_NAME: &str = "settings.json";
pub const SPLIT_DLL_NAME: &str = "edgeplus-split.dll";
const THEMES_FOLDER_NAME: &str = "themes";
const THEME_KEYS_FOLDER_NAME: &str = "keys";
const TEMPORARY_SUFFIX: &str = ".tmp";
const PAUSED_TOAST_FILE_NAME: &str = "paused-toast-build.txt";
const UPDATE_FOLDER_NAME: &str = "update";
const WINDOW_FOLDER_NAME: &str = "window";

#[must_use]
pub fn local_app_data() -> PathBuf {
    std::env::var_os("LOCALAPPDATA").map_or_else(|| PathBuf::from("."), PathBuf::from)
}

/// The desired state the DLL reads at load and on every change.
#[must_use]
pub fn config_file() -> PathBuf {
    let mut path = local_app_data().into_os_string();
    path.push(shared::CONFIG_FILE_RELATIVE_PATH);
    PathBuf::from(path)
}

#[must_use]
pub fn data_root() -> PathBuf {
    config_file().parent().map(Path::to_path_buf).unwrap_or_default()
}

#[must_use]
pub fn settings() -> PathBuf {
    data_root().join(SETTINGS_FILE_NAME)
}

/// Unpacked here for injection into Edge.
#[must_use]
pub fn split_dll() -> PathBuf {
    data_root().join(SPLIT_DLL_NAME)
}

/// The build the paused-features toast last spoke of, so it is raised once per
/// build.
#[must_use]
pub fn paused_toast_build() -> PathBuf {
    data_root().join(PAUSED_TOAST_FILE_NAME)
}

/// A new release's exe is downloaded and checked here until the user restarts
/// into it.
#[must_use]
pub fn update_folder() -> PathBuf {
    data_root().join(UPDATE_FOLDER_NAME)
}

/// Local theme `.crx` files are copied here to install by path.
#[must_use]
pub fn themes_root() -> PathBuf {
    data_root().join(THEMES_FOLDER_NAME)
}

/// Kept so a theme packed from a folder or `.zip` and added again keeps its
/// extension id.
#[must_use]
pub fn theme_keys() -> PathBuf {
    themes_root().join(THEME_KEYS_FOLDER_NAME)
}

/// A published exe unpacks the window (Node and its scripts) here.
#[must_use]
pub fn window_root() -> PathBuf {
    data_root().join(WINDOW_FOLDER_NAME)
}

/// Written whole then swapped in, so a reader (the DLL, Edge, the window) never
/// sees half a file.
pub fn write_atomically(path: &Path, contents: &[u8]) -> io::Result<()> {
    if let Some(folder) = path.parent() {
        fs::create_dir_all(folder)?;
    }
    let mut temporary = path.as_os_str().to_owned();
    temporary.push(TEMPORARY_SUFFIX);
    fs::write(&temporary, contents)?;
    fs::rename(&temporary, path)
}
