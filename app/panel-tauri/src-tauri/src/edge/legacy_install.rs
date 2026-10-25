//! What an install from before the rename leaves behind (the project was called
//! fix-edge): its split DLL, still loaded in Edge and listening on its own
//! unload event, and its data folder. These names are the old ones on purpose;
//! every current name is Edge+'s. Runs before anything is injected or read, so
//! the old DLL is gone before the new one comes in (two would hook the same
//! code twice) and the settings are in place before the window reads them.

use std::thread;
use std::time::Duration;

use crate::edge::{edge_install, legacy_data};
use crate::platform::{panel_log, paths};

const DATA_FOLDER_NAME: &str = "FixEdge";
const DLL_NAME: &str = "fixedge-split.dll";
/// Files the old install left that nothing reads any more.
const UNUSED_FILES: [&str; 2] = [DLL_NAME, "fixedge-config.bin"];
const UNLOAD_EVENT: &str = r"Local\fixedge-split-unload";
const UNLOAD_POLLS: u32 = 40;
const UNLOAD_POLL_INTERVAL: Duration = Duration::from_millis(100);

pub fn run() {
    if edge_install::is_simulated() {
        return;
    }
    eject_dll();
    carry_data();
}

/// A process the old DLL is still hooked in is left alone: injecting the new
/// one there would hook it twice. It clears when that Edge restarts.
#[must_use]
pub fn is_dll_loaded_in(pid: u32) -> bool {
    edge_native::module_path(pid, DLL_NAME).is_some()
}

fn eject_dll() {
    edge_native::signal_event(UNLOAD_EVENT);
    for _ in 0..UNLOAD_POLLS {
        if edge_native::pids_with_module(DLL_NAME).is_empty() {
            return;
        }
        thread::sleep(UNLOAD_POLL_INTERVAL);
    }
    // The event exists exactly while the DLL is live. Without it a module still
    // mapped is a leftover whose hooks are gone, which only needs freeing.
    if edge_native::is_event_open(UNLOAD_EVENT) {
        panel_log::write("the pre-rename split DLL did not unload; it stays until Edge restarts");
        return;
    }
    for pid in edge_native::pids_with_module(DLL_NAME) {
        edge_native::free_module(pid, DLL_NAME);
    }
}

fn carry_data() {
    let legacy_root = paths::local_app_data().join(DATA_FOLDER_NAME);
    let settings = paths::settings();
    let Some(settings_file_name) = settings.file_name().and_then(|name| name.to_str()) else {
        return;
    };
    let is_carried = legacy_data::carry(
        &legacy_root,
        &paths::data_root(),
        settings_file_name,
        &UNUSED_FILES,
    );
    if is_carried {
        panel_log::write(&format!("data carried over from {}", legacy_root.display()));
    }
}
