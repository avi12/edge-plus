//! Everything the window can ask for; the window never touches settings, Edge
//! or the DLL itself. Each can block (an injection waits for Edge), so the core
//! runs them off its message thread.

use std::path::Path;

use crate::dll::{dll_channel, dll_sync};
use crate::edge::{dev_mode_prompt, edge_build, edge_watch};
use crate::platform::action_error::{ActionError, ActionResult};
use crate::platform::background_service;
use crate::platform::settings::{self, ColorMode, Feature, Settings};
use crate::setup::{start_with_windows, update_watch};
use crate::themes::theme_library;
use crate::ui::panel_state::PanelState;

#[must_use]
pub fn load() -> PanelState {
    read(&Settings::load())
}

pub fn set_feature(feature: Feature, is_on: bool) -> ActionResult<PanelState> {
    let _gate = settings::gate();
    let mut settings = Settings::load();
    settings.features.set(feature, is_on);
    settings.save()?;
    if feature != Feature::QuietDevMode {
        dll_sync::apply(&settings);
    } else if is_on {
        arm_dev_mode_waiter();
    }
    Ok(read(&settings))
}

pub fn set_start_with_windows(is_on: bool) -> ActionResult<PanelState> {
    start_with_windows::set(is_on)?;
    Ok(load())
}

/// Turning it on also injects into an Edge that is open without the DLL. Like
/// inject, it returns once the DLL is live, so the state shows the outcome.
pub fn set_auto_inject(is_on: bool) -> ActionResult<PanelState> {
    let _gate = settings::gate();
    let mut settings = Settings::load();
    settings.auto_inject = is_on;
    settings.save()?;
    if is_on {
        dll_sync::apply_and_wait_for_load(&settings);
    }
    Ok(read(&settings))
}

/// Turning it on looks for a newer release at once, so the card can appear
/// without waiting for the next scheduled check.
pub fn set_auto_update(is_on: bool) -> ActionResult<PanelState> {
    let _gate = settings::gate();
    let mut settings = Settings::load();
    settings.auto_update = is_on;
    settings.save()?;
    if is_on {
        background_service::check_for_update_soon();
    }
    Ok(read(&settings))
}

/// The Inject button: the DLL into every Edge browser process missing it. The
/// returned state's is_split_loaded is the outcome the button shows.
pub fn inject() -> ActionResult<PanelState> {
    let _gate = settings::gate();
    let settings = Settings::load();
    dll_sync::apply_and_wait_for_load(&settings);
    Ok(read(&settings))
}

pub fn add_store_theme(url: &str) -> ActionResult<PanelState> {
    theme_library::add_store(url)?;
    Ok(load())
}

pub fn add_local_theme(path: &Path) -> ActionResult<PanelState> {
    theme_library::add_local(path)?;
    Ok(load())
}

pub fn remove_theme(id: &str) -> ActionResult<PanelState> {
    theme_library::remove(id)?;
    Ok(load())
}

pub fn reorder_themes(ids: &[String]) -> ActionResult<PanelState> {
    theme_library::reorder(ids)?;
    Ok(load())
}

pub fn assign_theme(mode: ColorMode, id: &str) -> ActionResult<PanelState> {
    theme_library::assign(mode, id)?;
    Ok(load())
}

/// Puts the downloaded release in place and starts it; the caller then ends
/// this process. The new exe swaps its own DLL into Edge, so no Edge restart.
pub fn restart_to_update() -> ActionResult<()> {
    let update = background_service::pending_update()
        .ok_or_else(|| ActionError::Internal("no update is ready to install".to_owned()))?;
    update_watch::restart_into(&update)?;
    Ok(())
}

fn arm_dev_mode_waiter() {
    if let Ok(executable) = std::env::current_exe() {
        let _ = edge_native::start_detached(&dev_mode_prompt::waiter_command_line(&executable));
    }
}

fn read(settings: &Settings) -> PanelState {
    PanelState {
        features: settings.features,
        theming: settings.theming.clone(),
        edge_themes: theme_library::edge_themes_not_in(&settings.theming),
        is_edge_running: !edge_native::browser_pids().is_empty(),
        is_start_with_windows: start_with_windows::is_enabled(),
        is_split_loaded: dll_channel::is_loaded(),
        is_split_wanted: dll_sync::is_wanted(settings),
        is_auto_inject: settings.auto_inject,
        is_auto_update: settings.auto_update,
        edge_build: edge_watch::current_build(),
        supported_edge_build: edge_build::pinned(),
    }
}
