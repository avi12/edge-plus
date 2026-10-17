//! The Fix Edge panel: a Tauri app that toggles Edge's live feature switches
//! (the N-pane native split, light/dark theming, the developer-mode prompt and
//! the web handlers for email and calendar links)
//! and drives the injected DLL. The web UI (Svelte + Fluent) calls the commands
//! in [`commands`]; every Win32 action is isolated in the `edge-native` crate so
//! this crate keeps `unsafe_code = "forbid"`.

mod commands;
mod edge_build;
mod edge_prefs;
mod edge_watch;
mod paths;
mod payload;
mod settings;
mod store_url;
mod watch;

/// The switch that re-runs the panel headless as the `Preferences` waiter.
pub const PREFERENCES_WAITER_ARG: &str = "--edit-preferences-after-exit";

/// Run the headless waiter instead of the GUI (see `main`): once Edge has
/// exited, write what the settings ask for into every profile's `Preferences`.
/// The settings are read only then, so the latest switches win.
pub fn run_preferences_waiter() {
    let user_data = edge_native::default_user_data();
    if !edge_native::wait_for_edge_exit(&user_data) {
        return;
    }
    let settings = settings::Settings::load();
    let now = edge_native::now_microseconds_since_1601();
    edge_native::edit_every_profile(&user_data, |preferences| {
        edge_prefs::apply(&settings, preferences, now);
    });
}

/// Start the panel window.
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_single_instance::init(|_app, _argv, _cwd| {}))
        .setup(|app| {
            payload::unpack();
            commands::arm_preferences_waiter_if_needed();
            edge_watch::start(app.handle().clone());
            watch::start(app.handle().clone());
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::load_state,
            commands::take_edge_update,
            commands::set_split,
            commands::set_theming,
            commands::set_quiet_dev_mode,
            commands::set_web_handlers,
            commands::add_store_theme,
            commands::add_local_theme,
            commands::remove_theme,
            commands::set_mode_theme
        ])
        .run(tauri::generate_context!())
        .expect("error while running the Fix Edge panel");
}
