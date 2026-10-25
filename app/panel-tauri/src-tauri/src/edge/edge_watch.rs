//! Edge swaps an update in only when it restarts, so the build can only change
//! when a new browser process appears or the launcher is swapped: both trigger
//! a build check before anything is injected.

use crate::edge::edge_build::BuildChange;
use crate::edge::file_version::FileVersion;

pub struct EdgeWatch {}

impl EdgeWatch {
    /// Both callbacks run on the watch thread. `on_browsers_changed` runs
    /// whenever the set of browser processes changes, after a new one has been
    /// brought in line.
    pub fn start(
        on_build_changed: impl Fn(BuildChange) + Send + Sync + 'static,
        on_browsers_changed: impl Fn() + Send + Sync + 'static,
    ) -> Self {
        let _ = (on_build_changed, on_browsers_changed);
        todo!()
    }

    /// Waits for a poll in progress; later calls do nothing.
    pub fn stop(&self) {
        todo!()
    }
}

#[must_use]
pub fn build_of(pid: u32) -> Option<FileVersion> {
    let _ = pid;
    todo!()
}

/// The running build, else the installed one Edge will run when it next
/// starts.
#[must_use]
pub fn current_build() -> Option<FileVersion> {
    todo!()
}
