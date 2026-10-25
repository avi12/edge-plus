//! One Edge+ process per user, for its whole life: the background service
//! (Edge and theme watchers, the update check) runs from start, and the window
//! is a separate process opened on demand.
//!
//! * A boot launch (`--background`, from the Run entry `start_with_windows`
//!   writes) opens no window: it is the always-on listener.
//! * Launching Edge+ again, or clicking its toast, opens the window from this
//!   same process.
//! * Closing the window keeps the listener while Edge+ starts with Windows, and
//!   ends the process otherwise.

use std::thread;
use std::time::Duration;

use tauri::RunEvent;

use crate::dll::payload;
use crate::edge::{legacy_cleanup, legacy_install};
use crate::platform::{app_identity, background_service, panel_log, switches};
use crate::setup::{start_with_windows, startup_entry, update_install};
use crate::ui::window_host;

/// Long enough for start-up's work to settle before the working set is trimmed.
const TRIM_DELAY: Duration = Duration::from_secs(5);

pub fn run(is_boot_launch: bool) {
    let app = tauri::Builder::default()
        .plugin(tauri_plugin_single_instance::init(
            |_app, command_line, _folder| {
                // A boot launch finding Edge+ already running has nothing to add,
                // and handing it over would open the window.
                if !switches::is_present(&command_line, switches::BACKGROUND) {
                    window_host::show();
                }
            },
        ))
        .setup(move |_app| {
            start_background();
            window_host::init(on_window_closed);
            if !is_boot_launch {
                window_host::show();
                return Ok(());
            }
            // A relaunch after an update is a boot launch too, and needs no
            // listener when Edge+ no longer starts with Windows.
            if !is_resident() {
                exit();
            }
            trim_when_settled();
            Ok(())
        })
        .build(tauri::generate_context!())
        .expect("the Tauri app builds");
    app.run(|_app, event| {
        // With no webview window, Tauri would end the loop at once.
        if let RunEvent::ExitRequested { api, code, .. } = event
            && code.is_none()
        {
            api.prevent_exit();
        }
    });
}

/// Idempotent. The pre-rename install is out of the way first, then the DLL is
/// unpacked before the watcher can inject it.
fn start_background() {
    legacy_install::run();
    legacy_cleanup::run();
    if let Ok(executable) = std::env::current_exe() {
        // What an update swapped out, now that nothing is running it.
        update_install::delete_leftovers(&executable);
        match startup_entry::migrate_to(&executable) {
            Ok(true) => panel_log::write(&format!(
                "start at sign-in moved from an earlier panel to {}",
                executable.display()
            )),
            Ok(false) => {}
            Err(error) => panel_log::write_error(
                "start at sign-in could not be moved from an earlier panel",
                &error,
            ),
        }
    }
    payload::unpack();
    app_identity::register();
    background_service::start(|event| window_host::on_service_event(&event));
}

/// While Edge+ starts with Windows, the process outlives its window as the
/// listener for Edge updates.
fn is_resident() -> bool {
    start_with_windows::is_enabled()
}

fn on_window_closed() {
    if !is_resident() {
        exit();
    }
    trim_when_settled();
}

fn exit() {
    background_service::stop();
    std::process::exit(0);
}

/// The windowless listener only watches Edge, so the pages a window or
/// start-up touched go back to Windows until a window opens again.
fn trim_when_settled() {
    thread::Builder::new()
        .name("trim".to_owned())
        .spawn(|| {
            thread::sleep(TRIM_DELAY);
            if !window_host::is_open() {
                edge_native::trim_working_set();
            }
        })
        .ok();
}
