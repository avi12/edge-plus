//! Start at sign-in is the Run key entry for the current user, which points at
//! the exe that is running.

use std::io;

use crate::setup::startup_entry::{self, StartupKeys};

#[must_use]
pub fn is_enabled() -> bool {
    startup_entry::is_enabled(&StartupKeys::user(), startup_entry::NAME)
}

pub fn set(is_on: bool) -> io::Result<()> {
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
