//! The toast that says which features an Edge update paused, raised once per
//! build and only while no window is open (the window says it itself).

use std::fs;
use std::sync::Mutex;

use crate::edge::edge_build::BuildChange;
use crate::edge::edge_install;
use crate::edge::edge_support;
use crate::edge::file_version::FileVersion;
use crate::platform::app_identity::APP_ID;
use crate::platform::{panel_log, paths};
use crate::ui::wording;

/// A simulation (`edge_install`) remembers in memory only, so the real record
/// stays as it was.
static SIMULATED_NOTIFIED: Mutex<Option<FileVersion>> = Mutex::new(None);

pub fn show_once_per_build(change: &BuildChange, on_click: impl Fn() + Send + Sync + 'static) {
    let notified_for = notified_for();
    if !edge_support::should_notify(&change.paused, Some(change.edge_build), notified_for) {
        return;
    }
    let body = wording::describe_paused(change.edge_build, change.supported_build, &change.paused);
    match edge_native::show_toast(APP_ID, wording::PAUSED_TOAST_TITLE, &body, on_click) {
        Ok(()) => remember(change.edge_build),
        // No toast is no reason to stop listening; the window still says it.
        Err(error) => panel_log::write_error("paused toast failed", &error),
    }
}

fn notified_for() -> Option<FileVersion> {
    if edge_install::is_simulated() {
        return *SIMULATED_NOTIFIED
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
    }
    fs::read_to_string(paths::paused_toast_build())
        .ok()
        .and_then(|text| FileVersion::parse(&text))
}

fn remember(build: FileVersion) {
    if edge_install::is_simulated() {
        *SIMULATED_NOTIFIED
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner) = Some(build);
        return;
    }
    let _ = paths::write_atomically(&paths::paused_toast_build(), build.to_string().as_bytes());
}
