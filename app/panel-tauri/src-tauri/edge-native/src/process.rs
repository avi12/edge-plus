//! Of all the msedge.exe processes (one browser per profile plus renderer, GPU
//! and utility children), only a browser process owns a visible, titled
//! top-level window: that is how the browser processes are told apart.

use std::path::PathBuf;
use std::time::Duration;

/// The pids of Edge's browser processes, or empty when Edge is not running.
#[must_use]
pub fn browser_pids() -> Vec<u32> {
    todo!()
}

/// The browser processes with `module` loaded.
#[must_use]
pub fn pids_with_module(module: &str) -> Vec<u32> {
    let _ = module;
    todo!()
}

/// Per process, so a second profile opened after the DLL loaded is still
/// injected.
#[must_use]
pub fn pids_missing_module(module: &str) -> Vec<u32> {
    let _ = module;
    todo!()
}

/// Edge keeps each version in its own folder, so this names the build the
/// process is actually running.
#[must_use]
pub fn module_path(pid: u32, module: &str) -> Option<PathBuf> {
    let _ = (pid, module);
    todo!()
}

#[must_use]
pub fn module_base(pid: u32, module: &str) -> Option<usize> {
    let _ = (pid, module);
    todo!()
}

/// Returns at once when the process is already gone.
pub fn wait_for_exit(pid: u32, timeout: Duration) {
    let _ = (pid, timeout);
    todo!()
}
