//! Watching local themes for edits.
//!
//! A theme added from a folder, `.zip` or `.crx` is polled for changes. When
//! one settles (the same new fingerprint on two polls in a row, so a save that
//! writes several files is taken once) the theme is re-checked and re-installed
//! through [`commands::refresh_local_theme`]; a broken edit is reported and
//! the theme Edge is showing is left alone.

use std::collections::hash_map::DefaultHasher;
use std::collections::HashMap;
use std::fs;
use std::hash::{Hash, Hasher};
use std::path::Path;
use std::time::{Duration, SystemTime};

use serde::Serialize;
use tauri::{AppHandle, Emitter};

use crate::commands;
use crate::settings::{Settings, ThemeSource};

const POLL_INTERVAL: Duration = Duration::from_secs(1);
/// The event the UI listens to for the outcome of a live refresh.
pub const WATCH_EVENT: &str = "theme-refreshed";

/// What happened to a theme that changed on disk.
#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RefreshOutcome {
    id: String,
    /// Why the edit was not applied; `None` when it was.
    error: Option<String>,
}

/// Start the polling thread.
pub fn start(app: AppHandle) {
    std::thread::spawn(move || watch(&app));
}

fn watch(app: &AppHandle) {
    let mut settled: HashMap<String, u64> = HashMap::new();
    let mut pending: HashMap<String, u64> = HashMap::new();
    loop {
        std::thread::sleep(POLL_INTERVAL);
        let settings = Settings::load();
        let is_watching = settings.features.theming;
        let local_themes = settings
            .theming
            .themes
            .iter()
            .filter(|theme| theme.source == ThemeSource::Local);
        for theme in local_themes {
            let Some(current) = fingerprint(Path::new(&theme.origin)) else {
                continue;
            };
            let Some(&known) = settled.get(&theme.id) else {
                settled.insert(theme.id.clone(), current);
                continue;
            };
            let is_unchanged = current == known;
            if is_unchanged || !is_watching {
                pending.remove(&theme.id);
                continue;
            }
            let is_settled = pending.get(&theme.id) == Some(&current);
            if !is_settled {
                pending.insert(theme.id.clone(), current);
                continue;
            }
            pending.remove(&theme.id);
            settled.insert(theme.id.clone(), current);
            let error = commands::refresh_local_theme(&theme.id, &theme.origin).err();
            let _ = app.emit(
                WATCH_EVENT,
                RefreshOutcome {
                    id: theme.id.clone(),
                    error,
                },
            );
        }
    }
}

// A hash of every file's path, size and modified time under `path`, or of the
// file itself; `None` when it is gone (a moved folder is not an edit).
fn fingerprint(path: &Path) -> Option<u64> {
    let mut hasher = DefaultHasher::new();
    hash_tree(path, &mut hasher)?;
    Some(hasher.finish())
}

fn hash_tree(path: &Path, hasher: &mut DefaultHasher) -> Option<()> {
    let metadata = fs::metadata(path).ok()?;
    if metadata.is_dir() {
        let mut children: Vec<_> = fs::read_dir(path)
            .ok()?
            .filter_map(|entry| entry.ok().map(|entry| entry.path()))
            .collect();
        children.sort();
        for child in children {
            child.hash(hasher);
            hash_tree(&child, hasher)?;
        }
    } else {
        metadata.len().hash(hasher);
        let modified = metadata.modified().ok()?;
        modified
            .duration_since(SystemTime::UNIX_EPOCH)
            .ok()?
            .hash(hasher);
    }
    Some(())
}
