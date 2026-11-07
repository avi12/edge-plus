//! A local theme's folder or file is watched, and an edit re-installed once it
//! settles (the same new fingerprint on two polls in a row), so a save that
//! writes several files is re-installed once.

use std::collections::HashMap;
use std::fs;
use std::hash::{DefaultHasher, Hash, Hasher};
use std::io;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex, PoisonError};
use std::time::Duration;

use crate::platform::actions::action_error::ActionError;
use crate::platform::actions::panel_log;
use crate::platform::service::poll_thread::PollThread;
use crate::platform::settings::{Settings, ThemeEntry, ThemeSource};
use crate::themes::theme_library;
use crate::ui::wording;

const THREAD_NAME: &str = "Theme watch";
const POLL_INTERVAL: Duration = Duration::from_secs(1);

/// `error` is None when the edit was applied.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ThemeRefresh {
    pub id: String,
    pub error: Option<String>,
}

pub struct ThemeWatch {
    thread: Mutex<PollThread>,
}

/// Per theme id: the fingerprint last taken, and a new one seen once.
#[derive(Default)]
struct Fingerprints {
    settled: HashMap<String, u64>,
    pending: HashMap<String, u64>,
}

impl ThemeWatch {
    /// `on_refreshed` runs on the watch thread.
    pub fn start(on_refreshed: impl Fn(ThemeRefresh) + Send + Sync + 'static) -> Self {
        let on_refreshed = Arc::new(on_refreshed);
        let fingerprints = Arc::new(Mutex::new(Fingerprints::default()));
        let poller = {
            let fingerprints = Arc::clone(&fingerprints);
            let on_refreshed = Arc::clone(&on_refreshed);
            move || poll(&fingerprints, on_refreshed.as_ref())
        };
        let first = {
            let poller = poller.clone();
            move || poller()
        };
        Self {
            thread: Mutex::new(PollThread::start(THREAD_NAME, POLL_INTERVAL, first, poller)),
        }
    }

    /// Waits for a poll in progress; later calls do nothing.
    pub fn stop(&self) {
        self.thread
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .stop();
    }
}

fn poll(fingerprints: &Mutex<Fingerprints>, on_refreshed: &impl Fn(ThemeRefresh)) {
    let settings = Settings::load();
    let is_watching = settings.features.theming;
    let local_themes = settings
        .theming
        .themes
        .iter()
        .filter(|theme| theme.source == ThemeSource::Local);
    let mut fingerprints = fingerprints.lock().unwrap_or_else(PoisonError::into_inner);
    for theme in local_themes {
        let Some(fingerprint) = fingerprint(Path::new(&theme.origin)) else {
            continue;
        };
        let Some(&known) = fingerprints.settled.get(&theme.id) else {
            fingerprints.settled.insert(theme.id.clone(), fingerprint);
            continue;
        };
        let is_unchanged = fingerprint == known;
        if is_unchanged || !is_watching {
            fingerprints.pending.remove(&theme.id);
            continue;
        }
        let is_settled = fingerprints.pending.get(&theme.id) == Some(&fingerprint);
        if !is_settled {
            fingerprints.pending.insert(theme.id.clone(), fingerprint);
            continue;
        }
        fingerprints.pending.remove(&theme.id);
        fingerprints.settled.insert(theme.id.clone(), fingerprint);
        on_refreshed(ThemeRefresh {
            id: theme.id.clone(),
            error: refresh(theme),
        });
    }
}

fn refresh(theme: &ThemeEntry) -> Option<String> {
    match theme_library::refresh_local(&theme.id, &theme.origin) {
        Ok(()) => None,
        Err(ActionError::User(message)) => Some(message),
        Err(ActionError::Internal(message)) => {
            panel_log::write_error(
                &format!("re-installing the edited theme {}", theme.id),
                &message,
            );
            Some(wording::THEME_FILES_UNREADABLE.to_owned())
        }
    }
}

/// None when it is gone: a moved folder is not an edit.
fn fingerprint(path: &Path) -> Option<u64> {
    let mut hasher = DefaultHasher::new();
    if path.is_file() {
        add_file(&mut hasher, path).ok()?;
        return Some(hasher.finish());
    }
    if !path.is_dir() {
        return None;
    }
    let mut files = Vec::new();
    collect_files(path, &mut files).ok()?;
    files.sort();
    for file in files {
        file.hash(&mut hasher);
        add_file(&mut hasher, &file).ok()?;
    }
    Some(hasher.finish())
}

fn add_file(hasher: &mut DefaultHasher, file: &Path) -> io::Result<()> {
    let metadata = fs::metadata(file)?;
    metadata.len().hash(hasher);
    metadata.modified()?.hash(hasher);
    Ok(())
}

fn collect_files(folder: &Path, files: &mut Vec<PathBuf>) -> io::Result<()> {
    for entry in fs::read_dir(folder)? {
        let path = entry?.path();
        if path.is_dir() {
            collect_files(&path, files)?;
        } else {
            files.push(path);
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_theme_fingerprint_follows_its_files_and_is_none_when_gone() {
        let root =
            std::env::temp_dir().join(format!("edgeplus-theme-watch-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(root.join("images")).expect("folder");
        fs::write(root.join("manifest.json"), "{}").expect("manifest");
        let before = fingerprint(&root).expect("a folder has a fingerprint");
        assert_eq!(
            Some(before),
            fingerprint(&root),
            "nothing changed, nothing moves"
        );
        fs::write(root.join("images").join("frame.png"), "png").expect("image");
        assert_ne!(Some(before), fingerprint(&root), "a new file is an edit");
        assert!(
            fingerprint(&root.join("manifest.json")).is_some(),
            "a file has one too"
        );
        fs::remove_dir_all(&root).expect("clean up");
        assert_eq!(None, fingerprint(&root), "a moved folder is not an edit");
    }
}
