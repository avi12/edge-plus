//! Start at sign-in: the package's startup task for a packaged copy, else the
//! Run key entry for the current user, which points at the exe that is running.

use std::io;

use crate::setup::package_install;
use crate::setup::startup_entry::{self, StartupKeys};

/// The task the manifest (app/package/AppxManifest.xml) declares, which runs
/// the launcher beside the exe.
const STARTUP_TASK_ID: &str = "EdgePlusStartup";

#[must_use]
pub fn is_enabled() -> bool {
    if package_install::is_packaged() {
        return edge_native::is_startup_task_enabled(STARTUP_TASK_ID).unwrap_or(false);
    }
    startup_entry::is_enabled(&StartupKeys::user(), startup_entry::NAME)
}

pub fn set(is_on: bool) -> io::Result<()> {
    if package_install::is_packaged() {
        return edge_native::set_startup_task(STARTUP_TASK_ID, is_on);
    }
    let keys = StartupKeys::user();
    if !is_on {
        return startup_entry::disable(&keys, startup_entry::NAME);
    }
    let executable = std::env::current_exe()?;
    startup_entry::enable(
        &keys,
        startup_entry::NAME,
        &startup_entry::run_command(&executable),
    )
}
