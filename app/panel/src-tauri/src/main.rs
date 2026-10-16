// The panel owns its entry point: armed with the waiter switch it runs headless
// (wait for Edge to exit, snooze the developer-mode prompt, quit) without ever
// showing a window; otherwise it starts the GUI.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    let is_dev_mode_waiter =
        std::env::args().any(|arg| arg == fix_edge_panel_lib::DEVMODE_WAITER_ARG);
    if is_dev_mode_waiter {
        fix_edge_panel_lib::run_dev_mode_waiter();
        return;
    }
    fix_edge_panel_lib::run();
}
