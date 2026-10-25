//! Breaks away from any job the core runs in (a terminal, an IDE), which would
//! otherwise end the child with it.

/// CreateProcessW detached with no window, breaking away from the job when the
/// job allows it. True when it started.
#[must_use]
pub fn start_detached(command_line: &str) -> bool {
    let _ = command_line;
    todo!()
}
