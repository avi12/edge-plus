//! What Edge wears, as the DLL reports it from inside Edge (`dll_wear.rs`).
//! Every change refreshes the "In use" badge. When the user went back to
//! Edge's default theme inside Edge ("Restore default"), the slot (light or
//! dark) of the Windows mode that is current now says "default theme" too, so
//! the list matches what Edge shows; nothing is uninstalled.
//!
//! The DLL tells who caused each change, so Edge+'s own switches (a Windows
//! mode flip, a pick in the list, the drops to the default Edge makes while one
//! settles) are never mistaken for the user's.

use std::sync::{Arc, Mutex, PoisonError};
use std::time::Duration;

use crate::dll::dll_sync;
use crate::dll::dll_wear::{self, Wear};
use crate::platform::actions::panel_log;
use crate::platform::service::poll_thread::PollThread;
use crate::platform::settings::{self, ColorMode, Settings};

const THREAD_NAME: &str = "Edge theme watch";
/// The report is a 76-byte file the DLL rewrites at each change.
const POLL_INTERVAL: Duration = Duration::from_millis(500);
const PERSONALIZE_KEY: &str = r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize";
const APPS_USE_LIGHT_THEME: &str = "AppsUseLightTheme";

/// What changed in what Edge wears.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WearChange {
    /// The user's own return to the default cleared this mode's slot.
    WentDefault(ColorMode),
    Other,
}

/// The Windows apps mode, as Edge follows it; light when the value is missing,
/// as Windows treats it.
#[must_use]
pub fn current_mode() -> ColorMode {
    let value = edge_native::read_dword(
        edge_native::Hive::CurrentUser,
        PERSONALIZE_KEY,
        APPS_USE_LIGHT_THEME,
    );
    match value {
        Some(0) => ColorMode::Dark,
        _ => ColorMode::Light,
    }
}

/// A report not seen before; the first one read is where the watch starts, not
/// a change (it may be from an earlier session).
fn is_new(wear: &Wear, last_stamp: Option<u32>) -> bool {
    last_stamp != Some(wear.stamp)
}

pub struct EdgeThemeWatch {
    thread: Mutex<PollThread>,
}

impl EdgeThemeWatch {
    /// `on_change` runs on the watch thread, after any slot it cleared was saved.
    pub fn start(on_change: impl Fn(WearChange) + Send + Sync + 'static) -> Self {
        let on_change = Arc::new(on_change);
        let last_stamp = Arc::new(Mutex::new(None));
        let first = {
            let last_stamp = Arc::clone(&last_stamp);
            move || {
                *last_stamp.lock().unwrap_or_else(PoisonError::into_inner) =
                    dll_wear::read().map(|wear| wear.stamp);
            }
        };
        let poller = move || poll(&last_stamp, on_change.as_ref());
        Self {
            thread: Mutex::new(PollThread::start(THREAD_NAME, POLL_INTERVAL, first, poller)),
        }
    }

    /// Waits for a poll in progress; later calls do nothing.
    pub fn stop(&self) {
        self.thread
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .stop();
    }
}

fn poll(last_stamp: &Mutex<Option<u32>>, on_change: &impl Fn(WearChange)) {
    let Some(wear) = dll_wear::read() else {
        return;
    };
    {
        let mut last = last_stamp.lock().unwrap_or_else(PoisonError::into_inner);
        if !is_new(&wear, *last) {
            return;
        }
        *last = Some(wear.stamp);
    }
    if wear.is_users_default() {
        let mode = current_mode();
        if clear(mode) {
            on_change(WearChange::WentDefault(mode));
            return;
        }
    }
    on_change(WearChange::Other);
}

/// Under the settings gate, so an apply in progress finishes first. False when
/// theming is off or the slot already says default. The DLL is told the new
/// settings so it does not put the old theme back at the next mode switch;
/// Edge already wears the default, so nothing visible moves.
fn clear(mode: ColorMode) -> bool {
    let _gate = settings::gate();
    let mut settings = Settings::load();
    let is_default_already = settings.theming.slot(mode).is_empty();
    if !settings.features.theming || is_default_already {
        return false;
    }
    settings.theming.clear_slot(mode);
    if let Err(error) = settings.save() {
        panel_log::write(&format!(
            "saving the default theme Edge went back to failed: {error}"
        ));
        return false;
    }
    dll_sync::apply(&settings);
    true
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::dll::dll_wear::WearCause;

    fn wear(stamp: u32) -> Wear {
        Wear {
            stamp,
            cause: WearCause::User,
            theme_id: String::new(),
        }
    }

    #[test]
    fn a_report_counts_once() {
        assert!(is_new(&wear(5), None));
        assert!(is_new(&wear(5), Some(4)));
        assert!(!is_new(&wear(5), Some(5)));
    }
}
