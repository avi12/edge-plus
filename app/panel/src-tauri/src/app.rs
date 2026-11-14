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
use crate::edge::legacy::{legacy_cleanup, legacy_install};
use crate::platform::actions::panel_log;
use crate::platform::identity::product;
use crate::platform::identity::{app_identity, switches};
use crate::platform::service::background_service;
use crate::setup::{install_layout, start_with_windows, startup_entry, update_watch};
use crate::ui::window_host;
use winui_svelte_launcher::Version;

/// Long enough for start-up's work to settle before the working set is trimmed.
const TRIM_DELAY: Duration = Duration::from_secs(5);

pub(crate) fn run(is_boot_launch: bool) -> tauri::Result<()> {
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
        .build(tauri::generate_context!())?;
    app.run(|_app, event| {
        // With no webview window, Tauri would end the loop at once.
        if let RunEvent::ExitRequested { api, code, .. } = event
            && code.is_none()
        {
            api.prevent_exit();
        }
    });
    Ok(())
}

/// Idempotent. The pre-rename install is out of the way first, then the DLL is
/// unpacked before the watcher can inject it.
fn start_background() {
    legacy_install::run();
    legacy_cleanup::run();
    move_startup_entry();
    payload::unpack();
    app_identity::register();
    background_service::start(|event| window_host::on_service_event(&event));
}

/// An installed copy removes the versions it replaced; every copy takes over an earlier panel's Run entry.
fn move_startup_entry() {
    prune_replaced_versions();
    let Ok(target) = start_with_windows::entry_target() else {
        return;
    };
    let moved = if install_layout::is_installed_run() {
        startup_entry::repoint_to(&target)
    } else {
        startup_entry::migrate_to(&target)
    };
    match moved {
        Ok(true) => panel_log::write(&format!(
            "start at sign-in moved from an earlier panel to {}",
            target.display()
        )),
        Ok(false) => {}
        Err(error) => panel_log::write_error(
            "start at sign-in could not be moved from an earlier panel",
            &error,
        ),
    }
}

/// The running version has started, so the ones it replaced can go.
fn prune_replaced_versions() {
    let Ok(running) = Version::parse(product::VERSION) else {
        return;
    };
    if install_layout::is_installed_run() {
        update_watch::prune(running);
    }
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
