//! The paused-features toast. An unpackaged app needs its AppUserModelID
//! registered (HKCU\Software\Classes\AppUserModelId\<id>, DisplayName and
//! IconUri) and set on the process before Windows shows its toasts.

use std::io;
use std::path::Path;

pub fn register_toast_app(app_id: &str, display_name: &str, icon: &Path) -> io::Result<()> {
    let _ = (app_id, display_name, icon);
    todo!()
}

/// `on_click` runs when the user clicks the toast while this process lives.
pub fn show_toast(
    app_id: &str,
    title: &str,
    body: &str,
    on_click: impl Fn() + Send + Sync + 'static,
) -> io::Result<()> {
    let _ = (app_id, title, body, on_click);
    todo!()
}
