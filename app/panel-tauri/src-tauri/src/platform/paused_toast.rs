//! The toast that says which features an Edge update paused, raised once per
//! build and only while no window is open (the window says it itself).

use std::fs;
use std::sync::Mutex;

use crate::edge::edge_build::BuildChange;
use crate::edge::edge_install;
use crate::edge::edge_support;
use crate::edge::file_version::FileVersion;
use crate::platform::{panel_log, paths, product};
use crate::ui::wording;

/// The toasts' sender, registered under HKCU at start.
pub const APP_ID: &str = "Avi.EdgePlus";
const ICON_FILE_NAME: &str = "edgeplus.ico";
const ICON: &[u8] = include_bytes!("../../icons/icon.ico");

/// A simulation (`edge_install`) remembers in memory only, so the real record
/// stays as it was.
static SIMULATED_NOTIFIED: Mutex<Option<FileVersion>> = Mutex::new(None);

/// Windows shows an unpackaged app's toasts only under a registered app id,
/// whose icon must be a file.
pub fn register() {
    let icon = paths::data_root().join(ICON_FILE_NAME);
    let registered = fs::create_dir_all(paths::data_root())
        .and_then(|()| fs::write(&icon, ICON))
        .and_then(|()| edge_native::register_toast_app(APP_ID, product::NAME, &icon));
    if let Err(error) = registered {
        panel_log::write_error("registering for toasts failed", &error);
    }
}

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
