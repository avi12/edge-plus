//! Snoozes Edge's "turn off extensions in developer mode" prompt. Edge rewrites
//! Preferences from memory while it runs, so the edit only sticks once Edge has
//! fully exited. The waiting runs in a headless re-run of the exe so it
//! survives the window closing.

use std::path::Path;

#[must_use]
pub fn waiter_command_line(executable: &Path) -> String {
    let _ = executable;
    todo!()
}

pub fn wait_for_edge_exit_then_snooze(user_data: &Path) {
    let _ = user_data;
    todo!()
}
