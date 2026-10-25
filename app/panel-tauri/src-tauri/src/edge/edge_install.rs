//! Edge installs an update into a new version folder and swaps its launcher on
//! the next restart, so the launcher's file version is the build Edge runs now
//! or will run next, readable even while Edge is closed.
//!
//! A dev seam: with EDGEPLUS_SIMULATE_EDGE_DIR set, that folder (holding any
//! versioned msedge.exe) is the Edge install, running processes are ignored,
//! and the core never injects or writes the DLL's config, so the real Edge is
//! never touched.

use std::path::{Path, PathBuf};

use crate::edge::file_version::FileVersion;

pub const SIMULATED_DIRECTORY_VARIABLE: &str = "EDGEPLUS_SIMULATE_EDGE_DIR";
pub const LAUNCHER_NAME: &str = "msedge.exe";

#[must_use]
pub fn simulated_directory() -> Option<PathBuf> {
    todo!()
}

#[must_use]
pub fn is_simulated() -> bool {
    simulated_directory().is_some()
}

#[must_use]
pub fn application_directory() -> Option<PathBuf> {
    todo!()
}

#[must_use]
pub fn installed_build(application_directory: Option<&Path>) -> Option<FileVersion> {
    let _ = application_directory;
    todo!()
}
