//! The data folder is a contract with the DLL (config.h, log.h), so its
//! location comes from the DLL's own constant (shared). Earlier panels kept it
//! in a folder of the project's old name; `legacy_install` carries that over.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use crate::platform::shared;

const SETTINGS_FILE_NAME: &str = "settings.json";
pub(crate) const SPLIT_DLL_NAME: &str = "edgeplus-split.dll";
const THEMES_FOLDER_NAME: &str = "themes";
const THEME_KEYS_FOLDER_NAME: &str = "keys";
const TEMPORARY_SUFFIX: &str = ".tmp";
const PAUSED_TOAST_FILE_NAME: &str = "paused-toast-build.txt";
const WINDOW_FOLDER_NAME: &str = "window";

#[must_use]
pub(crate) fn local_app_data() -> PathBuf {
    std::env::var_os("LOCALAPPDATA").map_or_else(|| PathBuf::from("."), PathBuf::from)
}

/// The desired state the DLL reads at load and on every change.
#[must_use]
pub(crate) fn config_file() -> PathBuf {
    under_local_app_data(shared::CONFIG_FILE_RELATIVE_PATH)
}

/// A file the DLL names by its path under `%LOCALAPPDATA%` (a leading `\`).
#[must_use]
pub(crate) fn under_local_app_data(relative_path: &str) -> PathBuf {
    let mut path = local_app_data().into_os_string();
    path.push(relative_path);
    PathBuf::from(path)
}

#[must_use]
pub(crate) fn data_root() -> PathBuf {
    config_file()
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_default()
}

#[must_use]
pub(crate) fn settings() -> PathBuf {
    data_root().join(SETTINGS_FILE_NAME)
}

/// Unpacked here for injection into Edge.
#[must_use]
pub(crate) fn split_dll() -> PathBuf {
    data_root().join(SPLIT_DLL_NAME)
}

/// The build the paused-features toast last spoke of, so it is raised once per
/// build.
#[must_use]
pub(crate) fn paused_toast_build() -> PathBuf {
    data_root().join(PAUSED_TOAST_FILE_NAME)
}

/// Local theme `.crx` files are copied here to install by path.
#[must_use]
pub(crate) fn themes_root() -> PathBuf {
    data_root().join(THEMES_FOLDER_NAME)
}

/// Kept so a theme packed from a folder or `.zip` and added again keeps its
/// extension id.
#[must_use]
pub(crate) fn theme_keys() -> PathBuf {
    themes_root().join(THEME_KEYS_FOLDER_NAME)
}

/// A published exe unpacks the window (Node and its scripts) here.
#[must_use]
pub(crate) fn window_root() -> PathBuf {
    data_root().join(WINDOW_FOLDER_NAME)
}

/// Windows matches file and folder names without regard to case (as .NET's
/// `OrdinalIgnoreCase` compares them), so names are compared in this folded
/// form.
#[must_use]
pub(crate) fn fold_case(name: &str) -> String {
    name.to_uppercase()
}

/// Written whole then swapped in, so a reader (the DLL, Edge, the window) never
/// sees half a file.
pub(crate) fn write_atomically(path: &Path, contents: &[u8]) -> io::Result<()> {
    if let Some(folder) = path.parent() {
        fs::create_dir_all(folder)?;
    }
    let mut temporary = path.as_os_str().to_owned();
    temporary.push(TEMPORARY_SUFFIX);
    fs::write(&temporary, contents)?;
    fs::rename(&temporary, path)
}
