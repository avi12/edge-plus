//! The one place that brings the running Edge in line with the settings.

use crate::platform::settings::Settings;

/// A settings change: with auto-inject off it reaches only the processes that
/// already hold the DLL.
pub fn apply(settings: &Settings) {
    let _ = settings;
    todo!()
}

/// A loaded DLL that is not this build's is ejected so the fresh one goes in.
pub fn sync_on_start() {
    todo!()
}

/// Returns once the DLL is live, or after a bounded wait when it never comes up.
pub fn apply_and_wait_for_load(settings: &Settings) {
    let _ = settings;
    todo!()
}

#[must_use]
pub fn is_wanted(settings: &Settings) -> bool {
    let _ = settings;
    todo!()
}
