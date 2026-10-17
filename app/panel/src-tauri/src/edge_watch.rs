//! Watching for Edge to (re)start, which is when an update lands.
//!
//! Edge installs an update into a new version folder and swaps it in when it
//! restarts, so the build only changes when a new browser process appears. At
//! panel start, and whenever a browser process appears that was not there on
//! the previous poll, the running build is checked through
//! [`edge_build::reconcile`] before anything is injected; features that
//! depend on a replaced `msedge.dll` are switched off and the UI is told.
//! Watching for new pids rather than for "none to some" also catches a restart
//! quicker than one poll.

use std::collections::HashSet;
use std::sync::{Mutex, PoisonError};
use std::time::Duration;

use tauri::{AppHandle, Emitter};

use crate::edge_build::BuildChange;
use crate::{commands, edge_build};

const POLL_INTERVAL: Duration = Duration::from_secs(2);
/// The event the UI listens to for a newly seen Edge build and the features it
/// switched off.
pub const EDGE_UPDATED_EVENT: &str = "edge-updated";

/// Check the running build, bring Edge in line with the settings, then keep
/// watching for new browser processes.
pub fn start(app: AppHandle) {
    std::thread::spawn(move || {
        check(&app);
        commands::sync_on_start();
        watch(&app);
    });
}

fn watch(app: &AppHandle) {
    let mut known: HashSet<u32> = edge_native::edge_browser_pids().into_iter().collect();
    loop {
        std::thread::sleep(POLL_INTERVAL);
        let current: HashSet<u32> = edge_native::edge_browser_pids().into_iter().collect();
        let has_new_process = !current.is_subset(&known);
        if has_new_process {
            check(app);
        }
        known = current;
    }
}

fn check(app: &AppHandle) {
    let Some(change) = edge_build::reconcile() else {
        return;
    };
    if !change.disabled.is_empty() {
        *UNSEEN.lock().unwrap_or_else(PoisonError::into_inner) = Some(change.clone());
    }
    let _ = app.emit(EDGE_UPDATED_EVENT, change);
}

// The last change that switched features off. The check at panel start runs
// before the UI is listening, so the UI also asks for it once it has loaded.
static UNSEEN: Mutex<Option<BuildChange>> = Mutex::new(None);

/// Hand over the last change that switched features off, once.
pub fn take_unseen() -> Option<BuildChange> {
    UNSEEN.lock().unwrap_or_else(PoisonError::into_inner).take()
}
