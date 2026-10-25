//! Swapping a downloaded exe in for the running one.

use std::path::Path;

/// After a restart, the exe that was swapped out is no longer running.
pub fn delete_leftovers(running: &Path) {
    let _ = running;
    todo!()
}
