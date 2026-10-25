//! Start at sign-in is the Run key entry for the current user, which points at
//! the exe that is running.

use std::io;

#[must_use]
pub fn is_enabled() -> bool {
    todo!()
}

pub fn set(is_on: bool) -> io::Result<()> {
    let _ = is_on;
    todo!()
}
