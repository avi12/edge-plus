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
use platform::actions::panel_log;
use platform::identity::product;
use platform::identity::switches;
use setup::{install_layout, installation};
use winui_svelte_launcher::{LifecycleEvent, Version};

fn main() {
    let command_line: Vec<String> = std::env::args().collect();
    if switches::is_present(&command_line, switches::DEV_MODE_WAITER) {
        dev_mode_prompt::wait_for_edge_exit_then_snooze(&edge_user_data::default_user_data());
        return;
    }
    let Ok(running) = Version::parse(product::VERSION) else {
        return;
    };
    let hook_arguments = command_line.get(1..).unwrap_or_default();
    if install_layout::is_installed_run() {
        // The launcher's hooks (--updated, --uninstall...) run and end.
        if let Some(result) = installation::dispatch_hook(hook_arguments, &running) {
            if let Err(error) = result {
                panel_log::write_error("a lifecycle hook failed", &error);
            }
            return;
        }
    } else if is_setup_run(&command_line) {
        if let Err(error) = installation::install_here(running) {
            panel_log::write_error("installing failed", &error);
        }
        return;
    }
    app::run(switches::is_present(&command_line, switches::BACKGROUND));
}

/// A published exe run from anywhere but the install installs itself, so
/// downloading and opening it is the whole setup; a build from a checkout runs
/// as it is, and --install forces the install.
fn is_setup_run(command_line: &[String]) -> bool {
    cfg!(window_kit) || switches::is_present(command_line, LifecycleEvent::Install.switch())
}
