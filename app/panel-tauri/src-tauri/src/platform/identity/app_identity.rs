//! Who Edge+ is to Windows: an `AppUserModelID` with a name and an icon. The
//! toasts are sent under it, and the window joins it, so the taskbar shows the
//! window as Edge+ with its icon rather than as Node.js.

use std::fs;
use std::path::PathBuf;

use crate::platform::actions::panel_log;
use crate::platform::identity::product;
use crate::platform::paths;

pub const APP_ID: &str = "Avi.EdgePlus";
const ICON_FILE_NAME: &str = "edgeplus.ico";
/// The one copy of the icon; the window loads the file written from it.
const ICON: &[u8] = include_bytes!("../../../icons/icon.ico");

/// Written by `register`, for Windows (an unpackaged app's toast icon must be
/// a file) and for the window's title bar.
#[must_use]
pub fn icon_path() -> PathBuf {
    paths::data_root().join(ICON_FILE_NAME)
}

/// At start, before any toast or window.
pub fn register() {
    let icon = icon_path();
    let registered = fs::create_dir_all(paths::data_root())
        .and_then(|()| fs::write(&icon, ICON))
        .and_then(|()| edge_native::register_toast_app(APP_ID, product::NAME, &icon));
    if let Err(error) = registered {
        panel_log::write_error("registering Edge+ with Windows failed", &error);
    }
}
