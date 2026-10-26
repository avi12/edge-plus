// The core decides the kind of launch before Tauri runs: the dev-mode waiter
// never starts the app at all.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod app;
mod dll;
mod edge;
mod platform;
mod setup;
mod themes;
mod ui;

use std::time::Duration;

use edge::{dev_mode_prompt, edge_user_data};
use platform::identity::switches;

/// How long a relaunch after an update waits for the exe it replaced to end.
const REPLACED_EXIT_WAIT: Duration = Duration::from_secs(30);

fn main() {
    let command_line: Vec<String> = std::env::args().collect();
    if switches::is_present(&command_line, switches::DEV_MODE_WAITER) {
        dev_mode_prompt::wait_for_edge_exit_then_snooze(&edge_user_data::default_user_data());
        return;
    }
    wait_for_replaced_process(&command_line);
    app::run(switches::is_present(&command_line, switches::BACKGROUND));
}

/// After an update the new exe starts before the old one has ended, and the
/// old one holds the single-instance lock until it does: waiting here, rather
/// than finding it and handing the launch over, is what lets the new one run.
fn wait_for_replaced_process(command_line: &[String]) {
    let replaced = switches::value_of(command_line, switches::RESTARTED_FROM)
        .and_then(|pid| pid.parse::<u32>().ok());
    if let Some(pid) = replaced {
        edge_native::wait_for_exit(pid, REPLACED_EXIT_WAIT);
    }
}
