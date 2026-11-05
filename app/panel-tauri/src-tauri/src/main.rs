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

use edge::{dev_mode_prompt, edge_user_data};
use platform::identity::switches;

fn main() {
    let command_line: Vec<String> = std::env::args().collect();
    if switches::is_present(&command_line, switches::DEV_MODE_WAITER) {
        dev_mode_prompt::wait_for_edge_exit_then_snooze(&edge_user_data::default_user_data());
        return;
    }
    app::run(switches::is_present(&command_line, switches::BACKGROUND));
}
