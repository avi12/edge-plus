//! The files the panel keeps under `%LOCALAPPDATA%\FixEdge`.
//!
//! These paths are a shared contract with the injected DLL, which reads the
//! config file and writes its log into the same folder (`log.h`,
//! `config.h`). The names must match the DLL's.

use std::path::PathBuf;

/// `%LOCALAPPDATA%\FixEdge`, the panel's data folder.
#[must_use]
pub fn root() -> PathBuf {
    let local = std::env::var_os("LOCALAPPDATA").map_or_else(|| PathBuf::from("."), PathBuf::from);
    local.join("FixEdge")
}

/// The user's switches and theme library.
#[must_use]
pub fn settings() -> PathBuf {
    root().join("settings.json")
}

/// The native-split DLL, unpacked here for injection into Edge.
#[must_use]
pub fn split_dll() -> PathBuf {
    root().join("fixedge-split.dll")
}

/// The desired-state file the DLL reads at load and on every change.
#[must_use]
pub fn config_file() -> PathBuf {
    root().join("fixedge-config.bin")
}

/// The signing keys of themes packed from a folder or `.zip`, so a theme added
/// again keeps its extension id.
#[must_use]
pub fn theme_keys() -> PathBuf {
    themes_root().join("keys")
}

/// Local theme `.crx` files are copied here to install by path.
#[must_use]
pub fn themes_root() -> PathBuf {
    root().join("themes")
}
