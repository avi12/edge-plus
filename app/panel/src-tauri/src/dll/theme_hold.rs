//! The theme Edge+ is installing without being asked to wear it. Edge makes a
//! theme it has just installed the active one; the DLL is told which one to
//! keep installed and off (config wire, `hold_id`), until the install has
//! landed. In memory only: after a restart nothing is being installed.

use std::sync::Mutex;
use std::thread;
use std::time::Duration;

use crate::dll::dll_sync;
use crate::platform::settings::{self, Settings};

/// Long enough for Edge to download, install and build the theme.
const HOLD_DURATION: Duration = Duration::from_secs(30);

static HELD: Mutex<String> = Mutex::new(String::new());

fn lock() -> std::sync::MutexGuard<'static, String> {
    HELD.lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
}

/// The id held now, empty for none.
#[must_use]
pub fn held() -> String {
    lock().clone()
}

/// Holds `id` and tells the running Edge, before the install that would make
/// it active starts; lets go again after `HOLD_DURATION`.
pub fn hold(id: &str) {
    id.clone_into(&mut lock());
    push();
    let id = id.to_owned();
    thread::spawn(move || {
        thread::sleep(HOLD_DURATION);
        let is_still_held = *lock() == id;
        if is_still_held {
            lock().clear();
            push();
        }
    });
}

/// Lets go of `id` at once, when a theme added without Shift is added again
/// with it.
pub fn release(id: &str) {
    let is_held = *lock() == id;
    if is_held {
        lock().clear();
        push();
    }
}

fn push() {
    let _gate = settings::gate();
    dll_sync::apply(&Settings::load());
}
