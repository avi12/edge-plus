//! The one place that brings the running Edge in line with the settings. The
//! DLL goes into every browser process except one on a build it has already
//! stood down on, and is sent the effective features (one it cannot run there
//! as off): it decides for itself, by signature, whether a build it was not
//! reversed against is safe, and says so (`dll_status`). When it stood down, or
//! no feature is left, a loaded copy unloads. A simulation (`edge_install`)
//! never touches Edge.

use std::thread;
use std::time::Duration;

use crate::dll::config_wire::DllConfig;
use crate::dll::{dll_channel, dll_status, payload};
use crate::edge::edge_watch;
use crate::edge::legacy::legacy_install;
use crate::edge::version::{edge_install, edge_support};
use crate::platform::paths;
use crate::platform::settings::{self, Features, Settings};

const UNLOAD_POLLS: u32 = 40;
const UNLOAD_POLL_INTERVAL: Duration = Duration::from_millis(100);
const LOAD_POLLS: u32 = 30;
const LOAD_POLL_INTERVAL: Duration = Duration::from_millis(100);

/// Which browser processes an apply may inject into.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Reach {
    EveryBrowser,
    Holders,
}

/// A settings change: with auto-inject off it reaches only the processes that
/// already hold the DLL, and an Edge without one waits for the Inject button.
pub(crate) fn apply(settings: &Settings) {
    if settings.auto_inject {
        apply_to(settings, Reach::EveryBrowser, &[]);
        return;
    }
    let holders = edge_native::pids_with_module(paths::SPLIT_DLL_NAME);
    apply_to(settings, Reach::Holders, &holders);
}

fn apply_to(settings: &Settings, reach: Reach, holders: &[u32]) {
    if edge_install::is_simulated() {
        return;
    }
    let effective = effective(settings);
    write_config_if_possible(settings, &effective);
    if !effective.needs_dll() {
        dll_channel::signal_unload();
        return;
    }
    // The unload event exists exactly while a live DLL does; a module still
    // mapped without it is a stale leftover that would swallow the injection.
    if !dll_channel::is_loaded() {
        for pid in edge_native::pids_with_module(paths::SPLIT_DLL_NAME) {
            edge_native::free_module(pid, paths::SPLIT_DLL_NAME);
        }
    }
    // After the stale copy is freed: a mapped DLL holds its file open, so the
    // fresh one cannot be written over it until then.
    payload::unpack();
    let missing: Vec<u32> = edge_native::pids_missing_module(paths::SPLIT_DLL_NAME)
        .into_iter()
        .filter(|&pid| edge_support::may_inject(edge_watch::build_of(pid)))
        .filter(|&pid| !legacy_install::is_dll_loaded_in(pid))
        .filter(|pid| reach == Reach::EveryBrowser || holders.contains(pid))
        .collect();
    if !missing.is_empty() {
        let _ = edge_native::inject_into(&missing, &paths::split_dll());
    }
    dll_channel::signal_config_changed();
}

/// A loaded DLL that is not this build's is ejected so the fresh one goes in:
/// a rebuilt core takes effect with no Edge restart. With auto-inject off a DLL
/// already loaded is still replaced, but an Edge without one waits for the
/// Inject button.
pub(crate) fn sync_on_start() {
    if edge_install::is_simulated() {
        return;
    }
    let _gate = settings::gate();
    let settings = Settings::load();
    if !is_wanted(&settings) {
        return;
    }
    // What an earlier DLL said about a build is not this one's word: it may place
    // what the last could not.
    let _ = dll_status::forget();
    // Taken before a stale DLL unloads, so its processes get the fresh one.
    let holders = edge_native::pids_with_module(paths::SPLIT_DLL_NAME);
    let is_stale = dll_channel::is_loaded() && !payload::is_unpacked_current();
    if is_stale {
        dll_channel::signal_unload();
        wait_for_unload();
    }
    let reach = if settings.auto_inject {
        Reach::EveryBrowser
    } else {
        Reach::Holders
    };
    apply_to(&settings, reach, &holders);
}

/// For a caller that reports the outcome (the Inject button, auto-inject):
/// returns once the DLL is live, or after a bounded wait when it never comes
/// up. `DllMain` creates the unload event before `LoadLibrary` returns, so the wait
/// is normally over at the first poll.
pub(crate) fn apply_and_wait_for_load(settings: &Settings) {
    apply_to(settings, Reach::EveryBrowser, &[]);
    let is_load_expected = !edge_install::is_simulated()
        && is_wanted(settings)
        && !injectable_browser_pids().is_empty();
    if is_load_expected {
        wait_for_load();
    }
}

/// Whether the settings want the DLL in Edge at all.
#[must_use]
pub(crate) fn is_wanted(settings: &Settings) -> bool {
    effective(settings).needs_dll()
}

fn injectable_browser_pids() -> Vec<u32> {
    edge_native::browser_pids()
        .into_iter()
        .filter(|&pid| edge_support::may_inject(edge_watch::build_of(pid)))
        .collect()
}

fn wait_for_load() {
    for _ in 0..LOAD_POLLS {
        if dll_channel::is_loaded() {
            return;
        }
        thread::sleep(LOAD_POLL_INTERVAL);
    }
}

fn wait_for_unload() {
    for _ in 0..UNLOAD_POLLS {
        let is_gone = edge_native::pids_with_module(paths::SPLIT_DLL_NAME).is_empty();
        if is_gone {
            return;
        }
        thread::sleep(UNLOAD_POLL_INTERVAL);
    }
}

/// A config file that cannot be written (a full disk, a locked folder) still
/// lets the load or unload go ahead; the DLL keeps the last state it read.
fn write_config_if_possible(settings: &Settings, effective: &Features) {
    let _ = dll_channel::write_config(&DllConfig::from_features(settings, effective));
}

fn effective(settings: &Settings) -> Features {
    edge_support::effective(
        &settings.features,
        edge_support::support_now(edge_watch::current_build()),
    )
}
