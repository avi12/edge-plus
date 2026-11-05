//! The package's startup task: how a packaged Edge+ starts at sign-in, in place
//! of a Run entry that would outlive an uninstalled package.

use std::io;

use windows::ApplicationModel::{StartupTask, StartupTaskState};
use windows::core::HSTRING;

fn task(id: &str) -> io::Result<StartupTask> {
    StartupTask::GetAsync(&HSTRING::from(id))
        .and_then(|operation| operation.get())
        .map_err(io::Error::other)
}

/// Fails when the package declares no task of that id.
pub fn is_startup_task_enabled(id: &str) -> io::Result<bool> {
    let state = task(id)?.State().map_err(io::Error::other)?;
    Ok(state == StartupTaskState::Enabled)
}

/// Fails when Windows keeps the task off, which only the user can undo from
/// Settings' startup apps.
pub fn set_startup_task(id: &str, is_on: bool) -> io::Result<()> {
    let task = task(id)?;
    if !is_on {
        return task.Disable().map_err(io::Error::other);
    }
    let state = task
        .RequestEnableAsync()
        .and_then(|operation| operation.get())
        .map_err(io::Error::other)?;
    if state != StartupTaskState::Enabled {
        return Err(io::Error::other(
            "Windows keeps the startup task off; turn it on in Settings > Apps > Startup",
        ));
    }
    Ok(())
}
