//! The Edge+ panel: a Tauri app that toggles Edge's live feature switches
//! (the N-pane native split, light/dark theming, the developer-mode prompt and
//! asking to handle links like Chrome) and drives the injected DLL. The web UI (Svelte + Fluent) calls the commands
//! in [`commands`]; every Win32 action is isolated in the `edge-native` crate so
//! this crate keeps `unsafe_code = "forbid"`.

mod commands;
mod edge_build;
mod edge_watch;
mod installed_themes;
mod paths;
mod payload;
mod settings;
mod store_url;
mod watch;

/// The switch that re-runs the panel headless as the dev-mode snooze waiter.
pub const DEVMODE_WAITER_ARG: &str = "--quiet-dev-mode-after-exit";

/// Run the headless dev-mode waiter instead of the GUI (see `main`).
pub fn run_dev_mode_waiter() {
    edge_native::wait_for_edge_exit_then_snooze(&edge_native::default_user_data());
}

/// Start the panel window.
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_single_instance::init(|_app, _argv, _cwd| {}))
        .setup(|app| {
            payload::unpack();
            edge_watch::start(app.handle().clone());
            watch::start(app.handle().clone());
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::load_state,
            commands::take_edge_update,
            commands::set_split,
            commands::set_theming,
            commands::set_assistant,
            commands::set_quiet_dev_mode,
            commands::set_protocol_handlers,
            commands::set_enhanced_find,
            commands::add_store_theme,
            commands::add_local_theme,
            commands::remove_theme,
            commands::set_mode_theme
        ])
        .run(tauri::generate_context!())
        .expect("error while running the Edge+ panel");
}
