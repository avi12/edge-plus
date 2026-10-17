//! The Tauri command surface the Svelte UI calls, and the one place that
//! reconciles "what the user wants" with "what Edge is running": the DLL is
//! loaded while either feature is on, its config is pushed live, and a theme is
//! installed through Edge's loader. Mirrors the old `FixEdgeBridge` +
//! `MainWindow.SyncDll`.

use std::os::windows::process::CommandExt;
use std::path::Path;
use std::process::Command;

use serde::Serialize;

use crate::edge_build::BuildChange;
use crate::settings::{Feature, Features, Mode, Settings, ThemeEntry, ThemeSource, Theming};
use crate::{edge_build, edge_watch, paths, payload, store_url, PREFERENCES_WAITER_ARG};

const REAPPLY_DELAYS: [std::time::Duration; 3] = [
    std::time::Duration::from_millis(1500),
    std::time::Duration::from_millis(2500),
    std::time::Duration::from_millis(5000),
];
const UNLOAD_POLLS: u32 = 40;
const UNLOAD_POLL_INTERVAL: std::time::Duration = std::time::Duration::from_millis(100);
const SPLIT_MODULE: &str = "fixedge-split.dll";
// DETACHED_PROCESS | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB: the
// Preferences waiter must outlive the panel window and Edge's job object.
const DETACHED_FLAGS: u32 = 0x0000_0008 | 0x0800_0000 | 0x0100_0000;

/// The whole state the UI renders.
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StateDto {
    features: Features,
    theming: Theming,
    edge_running: bool,
    split_loaded: bool,
    /// The `msedge.dll` build running Edge is on, when Edge is running.
    edge_build: Option<String>,
    /// The only build the split DLL supports.
    supported_edge_build: String,
    /// The features that cannot run on the running build; their switches are
    /// held off until Edge is on the supported build.
    unsupported: Vec<Feature>,
}

fn state_of(settings: &Settings) -> StateDto {
    let pinned = edge_build::pinned();
    let running = edge_build::running_build();
    StateDto {
        features: settings.features,
        theming: settings.theming.clone(),
        edge_running: !edge_native::edge_browser_pids().is_empty(),
        split_loaded: edge_native::is_split_loaded(),
        edge_build: running.map(|build| build.to_string()),
        supported_edge_build: pinned.to_string(),
        unsupported: running.map_or_else(Vec::new, |build| {
            edge_build::unsupported_features(build, pinned)
        }),
    }
}

// Bring the running Edge in line with the settings: write the config the DLL
// reads, load or unload the DLL, then signal a live re-read. The DLL only ever
// goes into a browser process running the build it was made for.
pub(crate) fn apply(settings: &Settings) {
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
    let pinned = edge_build::pinned();
    let missing: Vec<u32> = edge_native::edge_pids_missing_module(SPLIT_MODULE)
        .into_iter()
        .filter(|&pid| edge_build::build_of(pid) == Some(pinned))
        .collect();
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

/// The Edge update that switched features off before the UI was listening, if
/// any; handed over once.
#[tauri::command]
pub fn take_edge_update() -> Option<BuildChange> {
    edge_watch::take_unseen()
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
        arm_preferences_waiter();
    }
    Ok(state_of(&settings))
}

/// Gmail opens email links and Google Calendar calendar links, from Edge's
/// next start. Turning it off also needs Edge to exit, to remove the entries.
#[tauri::command]
pub fn set_web_handlers(enabled: bool) -> Result<StateDto, String> {
    let mut settings = Settings::load();
    settings.features.web_handlers = enabled;
    settings.save().map_err(|error| error.to_string())?;
    arm_preferences_waiter();
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
    let source = Path::new(&path);
    let (id, version, crx) = local_theme_crx(source).map_err(|error| error.to_string())?;
    let stored = paths::themes_root().join(format!("{id}.crx"));
    std::fs::create_dir_all(paths::themes_root()).map_err(|error| error.to_string())?;
    std::fs::write(&stored, crx).map_err(|error| error.to_string())?;
    edge_native::install_local(&id, &stored, &version).map_err(|error| error.to_string())?;
    let name = Path::new(&path)
        .file_stem()
        .map_or_else(|| id.clone(), |stem| stem.to_string_lossy().into_owned());
    let mut settings = Settings::load();
    add_or_replace(
        &mut settings.theming,
        ThemeEntry {
            id,
            name,
            source: ThemeSource::Local,
            origin: path,
        },
    );
    persist_and_apply(&settings)
}

/// Re-install a local theme whose source changed on disk, then have Edge show
/// the new copy. A theme that fails its checks is refused with the reason, and
/// what Edge has installed is left as it was.
///
/// # Errors
/// Returns why the edited theme cannot be applied.
pub fn refresh_local_theme(id: &str, origin: &str) -> Result<(), String> {
    let (packed_id, version, crx) =
        local_theme_crx(Path::new(origin)).map_err(|error| error.to_string())?;
    if packed_id != id {
        return Err("The theme's identity changed; add it again.".to_owned());
    }
    let stored = paths::themes_root().join(format!("{id}.crx"));
    std::fs::write(&stored, crx).map_err(|error| error.to_string())?;
    edge_native::install_local(id, &stored, &version).map_err(|error| error.to_string())?;
    let settings = Settings::load();
    apply(&settings);
    // Edge installs the new version a moment later; ask the DLL to re-apply the
    // active theme once that has landed.
    for delay in REAPPLY_DELAYS {
        std::thread::sleep(delay);
        edge_native::signal_config_changed();
    }
    Ok(())
}

// A local theme's id, version and `.crx` bytes: a `.crx` is used as it is, a
// folder or `.zip` is packed and signed first.
fn local_theme_crx(source: &Path) -> std::io::Result<(String, String, Vec<u8>)> {
    let is_crx = source
        .extension()
        .is_some_and(|extension| extension.eq_ignore_ascii_case("crx"));
    if is_crx {
        let bytes = std::fs::read(source)?;
        let info = edge_native::read_crx(&bytes)?;
        return Ok((info.id, info.version, bytes));
    }
    let packed = edge_native::pack_theme(source, &paths::theme_keys())?;
    Ok((packed.id, packed.version, packed.crx))
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

/// At startup, re-arm the waiter for a switch that edits `Preferences`, so its
/// edit lands even if the last waiter never saw Edge exit (a reboot ends it).
pub fn arm_preferences_waiter_if_needed() {
    let features = Settings::load().features;
    let has_preferences_edit = features.quiet_dev_mode || features.web_handlers;
    if has_preferences_edit {
        arm_preferences_waiter();
    }
}

// Re-run this same executable headless to do the waiting, detached so it
// outlives the panel window.
fn arm_preferences_waiter() {
    let Ok(executable) = std::env::current_exe() else {
        return;
    };
    let _ = Command::new(executable)
        .arg(PREFERENCES_WAITER_ARG)
        .creation_flags(DETACHED_FLAGS)
        .spawn();
}
