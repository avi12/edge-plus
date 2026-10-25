//! Asks the update manifest for a newer release at start and every six hours,
//! and downloads it in the background.

use std::io;
use std::path::PathBuf;

/// A newer release whose file is downloaded and checked, waiting for the user
/// to restart into it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ReadyUpdate {
    pub version: String,
    pub file: PathBuf,
    /// The MSI, for a copy the MSI installed; else the portable exe.
    pub is_installer: bool,
}

pub struct UpdateWatch {}

impl UpdateWatch {
    /// `on_ready` runs on the watch thread.
    pub fn start(on_ready: impl Fn(ReadyUpdate) + Send + Sync + 'static) -> Self {
        let _ = on_ready;
        todo!()
    }

    /// Cancels a download in progress; later calls do nothing.
    pub fn stop(&self) {
        todo!()
    }

    /// The switch was turned on: look now rather than at the next check.
    pub fn check_soon(&self) {
        todo!()
    }

    #[must_use]
    pub fn pending(&self) -> Option<ReadyUpdate> {
        todo!()
    }
}

/// Puts the pending update in place and starts it (the portable exe), or hands
/// the MSI to a hidden PowerShell that runs it once this process exits; the
/// caller then ends this process.
pub fn restart_into(update: &ReadyUpdate) -> io::Result<()> {
    let _ = update;
    todo!()
}
