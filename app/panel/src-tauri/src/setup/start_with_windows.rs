//! Start at sign-in: the Run key entry for the current user. An installed copy
//! starts the launcher in the root, which starts the newest version, so the
//! entry survives updates; any other copy points at the exe that is running.

use std::io;
use std::path::PathBuf;

use crate::setup::install_layout;
use crate::setup::startup_entry::{self, StartupKeys};

#[must_use]
pub fn is_enabled() -> bool {
    startup_entry::is_enabled(&StartupKeys::user(), startup_entry::NAME)
}

/// What the Run entry starts for this copy.
pub fn entry_target() -> io::Result<PathBuf> {
    if install_layout::is_installed_run() {
        return Ok(install_layout::launcher(&install_layout::root()));
    }
    std::env::current_exe()
}

pub fn set(is_on: bool) -> io::Result<()> {
    let keys = StartupKeys::user();
    if !is_on {
        return startup_entry::disable(&keys, startup_entry::NAME);
    }
    startup_entry::enable(
        &keys,
        startup_entry::NAME,
        &startup_entry::run_command(&entry_target()?),
    )
}
