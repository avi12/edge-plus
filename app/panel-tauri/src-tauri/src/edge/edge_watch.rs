//! Edge swaps an update in only when it restarts, so the build can only change
//! when a new browser process appears or the launcher is swapped: both trigger
//! a build check before anything is injected.

use std::collections::HashSet;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::{Mutex, PoisonError};
use std::time::{Duration, SystemTime};

use crate::dll::dll_sync;
use crate::edge::edge_build::{self, BuildChange};
use crate::edge::file_version::FileVersion;
use crate::edge::{edge_install, edge_support};
use crate::platform::poll_thread::PollThread;
use crate::platform::settings::{self, Settings};
use crate::platform::shared;

const THREAD_NAME: &str = "Edge watch";
const POLL_INTERVAL: Duration = Duration::from_secs(2);

type BuildChangedCallback = Box<dyn Fn(BuildChange) + Send + Sync>;
type BrowsersChangedCallback = Box<dyn Fn() + Send + Sync>;

pub struct EdgeWatch {
    thread: Mutex<Option<PollThread>>,
}

impl EdgeWatch {
    /// Both callbacks run on the watch thread. `on_browsers_changed` runs
    /// whenever the set of browser processes changes, after a new one has been
    /// brought in line.
    pub fn start(
        on_build_changed: impl Fn(BuildChange) + Send + Sync + 'static,
        on_browsers_changed: impl Fn() + Send + Sync + 'static,
    ) -> Self {
        let watcher = Arc::new(Watcher {
            on_build_changed: Box::new(on_build_changed),
            on_browsers_changed: Box::new(on_browsers_changed),
            state: Mutex::new(WatchState::default()),
        });
        let first = Arc::clone(&watcher);
        let thread = PollThread::start(
            THREAD_NAME,
            POLL_INTERVAL,
            move || first.begin(),
            move || watcher.poll(),
        );
        Self {
            thread: Mutex::new(Some(thread)),
        }
    }

    /// Waits for a poll in progress; later calls do nothing.
    pub fn stop(&self) {
        let thread = self
            .thread
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .take();
        if let Some(mut thread) = thread {
            thread.stop();
        }
    }
}

#[must_use]
pub fn build_of(pid: u32) -> Option<FileVersion> {
    let path = edge_native::module_path(pid, shared::EDGE_MODULE)?;
    FileVersion::of(&path)
}

#[must_use]
pub fn running_build() -> Option<FileVersion> {
    let builds: Vec<FileVersion> = edge_native::browser_pids()
        .into_iter()
        .filter_map(build_of)
        .collect();
    edge_build::judged(&builds, edge_build::pinned())
}

/// The running build, else the installed one Edge will run when it next
/// starts.
#[must_use]
pub fn current_build() -> Option<FileVersion> {
    if let Some(simulated) = edge_install::simulated_directory() {
        return edge_install::installed_build(Some(&simulated));
    }
    running_build()
        .or_else(|| edge_install::installed_build(edge_install::application_directory().as_deref()))
}

/// Records a new build and brings the DLL in line with it; None when the build
/// is unchanged.
pub fn reconcile() -> io::Result<Option<BuildChange>> {
    let Some(build) = current_build() else {
        return Ok(None);
    };
    let _gate = settings::gate();
    let mut settings = Settings::load();
    let previous = settings
        .last_edge_build
        .as_deref()
        .and_then(FileVersion::parse);
    let is_new_build = previous != Some(build);
    if !is_new_build {
        return Ok(None);
    }
    if !edge_install::is_simulated() {
        settings.last_edge_build = Some(build.to_string());
        settings.save()?;
        // A new build only shows once Edge restarted, so nothing is loaded:
        // with auto-inject off the DLL waits for the Inject button.
        if settings.auto_inject {
            dll_sync::apply(&settings);
        }
    }
    let pinned = edge_build::pinned();
    let paused = edge_support::paused(&settings.features, Some(build), pinned);
    Ok(Some(BuildChange {
        edge_build: build,
        previous_build: previous,
        supported_build: pinned,
        paused,
    }))
}

/// The launcher as last seen; a change means Edge swapped an update in.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct LauncherStamp {
    modified: Option<SystemTime>,
    size: u64,
}

impl LauncherStamp {
    /// None when the launcher is missing (mid-swap).
    fn read(launcher: &Path) -> Option<Self> {
        let metadata = fs::metadata(launcher).ok()?;
        Some(Self {
            modified: metadata.modified().ok(),
            size: metadata.len(),
        })
    }
}

#[derive(Default)]
struct WatchState {
    known_browser_pids: HashSet<u32>,
    /// Set once the install folder is found at start; None leaves the launcher
    /// unwatched.
    launcher: Option<PathBuf>,
    launcher_stamp: Option<LauncherStamp>,
}

struct Watcher {
    on_build_changed: BuildChangedCallback,
    on_browsers_changed: BrowsersChangedCallback,
    state: Mutex<WatchState>,
}

impl Watcher {
    fn begin(&self) {
        self.check();
        dll_sync::sync_on_start();
        let mut state = self.state.lock().unwrap_or_else(PoisonError::into_inner);
        state.known_browser_pids = edge_native::browser_pids().into_iter().collect();
        Self::watch_install(&mut state);
    }

    /// Catches an update applied while Edge is closed. The launcher's modified
    /// time and size are compared on every poll, where the C# panel had a
    /// file-system watcher on it.
    fn watch_install(state: &mut WatchState) {
        let Some(folder) = edge_install::application_directory() else {
            return;
        };
        if !folder.is_dir() {
            return;
        }
        let launcher = folder.join(edge_install::LAUNCHER_NAME);
        state.launcher_stamp = LauncherStamp::read(&launcher);
        state.launcher = Some(launcher);
    }

    fn poll(&self) {
        self.poll_install();
        self.poll_browsers();
    }

    fn poll_install(&self) {
        let has_launcher_changed = {
            let mut state = self.state.lock().unwrap_or_else(PoisonError::into_inner);
            let Some(launcher) = &state.launcher else {
                return;
            };
            let stamp = LauncherStamp::read(launcher);
            let has_changed = stamp != state.launcher_stamp;
            state.launcher_stamp = stamp;
            has_changed
        };
        if has_launcher_changed {
            self.check();
        }
    }

    /// A browser process only counts once it has its window (`browser_pids`),
    /// which the DLL needs to find when it loads.
    fn poll_browsers(&self) {
        let current: HashSet<u32> = edge_native::browser_pids().into_iter().collect();
        let known = self
            .state
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .known_browser_pids
            .clone();
        let has_new_process = !current.is_subset(&known);
        let has_changed = current != known;
        if has_new_process {
            self.check();
            inject_into_new_processes();
        }
        self.state
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .known_browser_pids = current;
        if has_changed {
            (self.on_browsers_changed)();
        }
    }

    /// A settings file that cannot be written is tried again on the next new
    /// process; a panic out of a background thread would end the watching.
    fn check(&self) {
        let Ok(Some(change)) = reconcile() else {
            return;
        };
        (self.on_build_changed)(change);
    }
}

/// Edge restarted on the same build: reconcile sees no change, so nothing else
/// would put the DLL back. With auto-inject off the window offers its Inject
/// button instead.
fn inject_into_new_processes() {
    let _gate = settings::gate();
    let settings = Settings::load();
    if settings.auto_inject {
        dll_sync::apply_and_wait_for_load(&settings);
    }
}
