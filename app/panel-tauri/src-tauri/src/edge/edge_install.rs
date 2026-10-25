//! Edge installs an update into a new version folder and swaps its launcher on
//! the next restart, so the launcher's file version is the build Edge runs now
//! or will run next, readable even while Edge is closed.
//!
//! A dev seam: with `EDGEPLUS_SIMULATE_EDGE_DIR` set, that folder (holding any
//! versioned msedge.exe) is the Edge install, running processes are ignored,
//! and the core never injects or writes the DLL's config, so the real Edge is
//! never touched.

use std::path::{Path, PathBuf};

use edge_native::Hive;

use crate::edge::file_version::FileVersion;

pub const SIMULATED_DIRECTORY_VARIABLE: &str = "EDGEPLUS_SIMULATE_EDGE_DIR";
pub const LAUNCHER_NAME: &str = "msedge.exe";
/// The key's default value is the launcher's path.
const APP_PATHS_KEY: &str = r"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe";

#[must_use]
pub fn simulated_directory() -> Option<PathBuf> {
    let folder = std::env::var_os(SIMULATED_DIRECTORY_VARIABLE)?;
    let is_blank = folder.to_string_lossy().trim().is_empty();
    if is_blank {
        return None;
    }
    Some(PathBuf::from(folder))
}

#[must_use]
pub fn is_simulated() -> bool {
    simulated_directory().is_some()
}

#[must_use]
pub fn application_directory() -> Option<PathBuf> {
    if let Some(simulated) = simulated_directory() {
        return Some(simulated);
    }
    let launcher = launcher(Hive::LocalMachine).or_else(|| launcher(Hive::CurrentUser))?;
    launcher.parent().map(Path::to_path_buf)
}

#[must_use]
pub fn installed_build(application_directory: Option<&Path>) -> Option<FileVersion> {
    FileVersion::of(&application_directory?.join(LAUNCHER_NAME))
}

/// An unreadable key reads as no install there.
fn launcher(hive: Hive) -> Option<PathBuf> {
    let path = PathBuf::from(edge_native::read_string(hive, APP_PATHS_KEY, None)?);
    path.is_file().then_some(path)
}

#[cfg(test)]
mod tests {
    use std::fs;

    use super::*;

    /// The seam: a fake install folder holding any versioned msedge.exe.
    /// Needs edge-native (file versions).
    #[test]
    fn the_installed_build_is_the_launchers_version() {
        let folder =
            std::env::temp_dir().join(format!("edgeplus-fake-edge-{}", std::process::id()));
        fs::create_dir_all(&folder).expect("create the fake install");
        let system = std::env::var_os("SystemRoot").expect("SystemRoot");
        let notepad = Path::new(&system).join("System32").join("notepad.exe");
        fs::copy(notepad, folder.join(LAUNCHER_NAME)).expect("copy a versioned exe");

        // Compared with the copy: Windows reports a System32 binary in place
        // through a version shim (6.2) that a copy outside it does not get.
        let launcher = FileVersion::of(&folder.join(LAUNCHER_NAME));
        let missing = installed_build(Some(&folder.join("nothing-here")));
        let installed = installed_build(Some(&folder));
        let _ = fs::remove_dir_all(&folder);

        assert!(
            launcher.is_some_and(|version| version.build > 0),
            "the fake launcher has a version"
        );
        assert_eq!(
            launcher, installed,
            "the launcher's file version is the installed build"
        );
        assert!(missing.is_none(), "no launcher, no build");
        assert!(installed_build(None).is_none(), "no install, no build");
    }
}
