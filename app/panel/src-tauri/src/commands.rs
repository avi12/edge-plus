//! The Tauri command surface the Svelte UI calls, and the one place that
//! reconciles "what the user wants" with "what Edge is running": the DLL is
//! loaded while either feature is on, its config is pushed live, and a theme is
//! installed through Edge's loader. Mirrors the old `FixEdgeBridge` +
//! `MainWindow.SyncDll`.

use std::os::windows::process::CommandExt;
use std::path::Path;
use std::process::Command;

use serde::Serialize;

use crate::settings::{Features, Mode, Settings, ThemeEntry, ThemeSource, Theming};
use crate::{paths, payload, store_url, DEVMODE_WAITER_ARG};

const UNLOAD_POLLS: u32 = 40;
const UNLOAD_POLL_INTERVAL: std::time::Duration = std::time::Duration::from_millis(100);
const SPLIT_MODULE: &str = "fixedge-split.dll";
// DETACHED_PROCESS | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB: the dev-mode
// waiter must outlive the panel window and Edge's job object.
const DETACHED_FLAGS: u32 = 0x0000_0008 | 0x0800_0000 | 0x0100_0000;

/// The whole state the UI renders.
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StateDto {
    features: Features,
    theming: Theming,
    edge_running: bool,
    split_loaded: bool,
}

fn state_of(settings: &Settings) -> StateDto {
    StateDto {
        features: settings.features,
        theming: settings.theming.clone(),
        edge_running: !edge_native::edge_browser_pids().is_empty(),
        split_loaded: edge_native::is_split_loaded(),
    }
}

// Bring the running Edge in line with the settings: write the config the DLL
// reads, load or unload the DLL, then signal a live re-read.
fn apply(settings: &Settings) {
    let split = settings.features.native_split;
    let theming = settings.features.theming;
    let _ = edge_native::write_config(
        &paths::config_file(),
        split,
        theming,
        &settings.theming.light,
        &settings.theming.dark,
    );
    if !(split || theming) {
        edge_native::signal_unload();
        return;
    }
    payload::unpack();
    // The unload event exists exactly while a live DLL does; a module still
    // mapped without it is a stale leftover that would swallow the injection.
    if !edge_native::is_split_loaded() {
        for pid in edge_native::edge_pids_with_module(SPLIT_MODULE) {
            edge_native::free_module(pid, SPLIT_MODULE);
        }
    }
    let missing = edge_native::edge_pids_missing_module(SPLIT_MODULE);
    if !missing.is_empty() {
        let _ = edge_native::inject_into(&missing, &paths::split_dll());
    }
    edge_native::signal_config_changed();
}

/// At startup: if the DLL loaded in Edge is not this build's, eject it and let
/// `apply` inject the fresh one, so a rebuilt panel takes effect with no Edge
/// restart.
pub fn sync_on_start() {
    let settings = Settings::load();
    let is_enabled = settings.features.native_split || settings.features.theming;
    let is_stale = edge_native::is_split_loaded() && !payload::is_unpacked_current();
    if is_enabled && is_stale {
        edge_native::signal_unload();
        for _ in 0..UNLOAD_POLLS {
            let is_gone = edge_native::edge_pids_with_module(SPLIT_MODULE).is_empty();
            if is_gone {
                break;
            }
            std::thread::sleep(UNLOAD_POLL_INTERVAL);
        }
    }
    if is_enabled {
        apply(&settings);
    }
}

fn persist_and_apply(settings: &Settings) -> Result<StateDto, String> {
    settings.save().map_err(|error| error.to_string())?;
    apply(settings);
    Ok(state_of(settings))
}

#[tauri::command]
pub fn load_state() -> StateDto {
    state_of(&Settings::load())
}

#[tauri::command]
pub fn set_split(enabled: bool) -> Result<StateDto, String> {
    let mut settings = Settings::load();
    settings.features.native_split = enabled;
    persist_and_apply(&settings)
}

#[tauri::command]
pub fn set_theming(enabled: bool) -> Result<StateDto, String> {
    let mut settings = Settings::load();
    settings.features.theming = enabled;
    persist_and_apply(&settings)
}

#[tauri::command]
pub fn set_quiet_dev_mode(enabled: bool) -> Result<StateDto, String> {
    let mut settings = Settings::load();
    settings.features.quiet_dev_mode = enabled;
    settings.save().map_err(|error| error.to_string())?;
    if enabled {
        arm_dev_mode_waiter();
    }
    Ok(state_of(&settings))
}

#[tauri::command]
pub fn add_store_theme(url: String) -> Result<StateDto, String> {
    let parsed = store_url::parse(&url)
        .ok_or_else(|| "That is not a Chrome Web Store or Edge Add-ons theme link.".to_owned())?;
    edge_native::install_store(&parsed.id, parsed.store).map_err(|error| error.to_string())?;
    let mut settings = Settings::load();
    add_or_replace(
        &mut settings.theming,
        ThemeEntry {
            id: parsed.id,
            name: parsed.name,
            source: ThemeSource::Store,
            origin: url,
        },
    );
    persist_and_apply(&settings)
}

#[tauri::command]
pub fn add_local_theme(path: String) -> Result<StateDto, String> {
    let bytes = std::fs::read(&path).map_err(|error| error.to_string())?;
    let info = edge_native::read_crx(&bytes).map_err(|error| error.to_string())?;
    let stored = paths::themes_root().join(format!("{}.crx", info.id));
    std::fs::create_dir_all(paths::themes_root()).map_err(|error| error.to_string())?;
    std::fs::copy(&path, &stored).map_err(|error| error.to_string())?;
    edge_native::install_local(&info.id, &stored, &info.version)
        .map_err(|error| error.to_string())?;
    let name = Path::new(&path).file_stem().map_or_else(
        || info.id.clone(),
        |stem| stem.to_string_lossy().into_owned(),
    );
    let mut settings = Settings::load();
    add_or_replace(
        &mut settings.theming,
        ThemeEntry {
            id: info.id,
            name,
            source: ThemeSource::Local,
            origin: path,
        },
    );
    persist_and_apply(&settings)
}

#[tauri::command]
pub fn remove_theme(id: String) -> Result<StateDto, String> {
    edge_native::uninstall(&id);
    let mut settings = Settings::load();
    settings.theming.themes.retain(|theme| theme.id != id);
    if settings.theming.light == id {
        settings.theming.light.clear();
    }
    if settings.theming.dark == id {
        settings.theming.dark.clear();
    }
    persist_and_apply(&settings)
}

#[tauri::command]
pub fn set_mode_theme(mode: String, id: String) -> Result<StateDto, String> {
    let mut settings = Settings::load();
    match parse_mode(&mode)? {
        Mode::Light => settings.theming.light = id,
        Mode::Dark => settings.theming.dark = id,
    }
    persist_and_apply(&settings)
}

fn parse_mode(mode: &str) -> Result<Mode, String> {
    match mode {
        "light" => Ok(Mode::Light),
        "dark" => Ok(Mode::Dark),
        _ => Err(format!("unknown mode: {mode}")),
    }
}

fn add_or_replace(theming: &mut Theming, entry: ThemeEntry) {
    if let Some(existing) = theming.themes.iter_mut().find(|theme| theme.id == entry.id) {
        *existing = entry;
    } else {
        theming.themes.push(entry);
    }
}

// Re-run this same executable headless to do the waiting, detached so it
// outlives the panel window.
fn arm_dev_mode_waiter() {
    let Ok(executable) = std::env::current_exe() else {
        return;
    };
    let _ = Command::new(executable)
        .arg(DEVMODE_WAITER_ARG)
        .creation_flags(DETACHED_FLAGS)
        .spawn();
}
