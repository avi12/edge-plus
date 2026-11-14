//! Asks the release feed (`update_feed`) for a newer release at start and then
//! every six hours, and stages it in the background (downloaded, checked and
//! unpacked into its own `app-x.y.z` folder) so the window can offer a restart
//! at once. Only an installed copy updates: a build run from a checkout is never
//! touched. Off (`Settings::auto_update`), Edge+ never contacts GitHub.

use std::fs::{self, File};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex, PoisonError};
use std::time::{Duration, SystemTime};

use winui_svelte_launcher::{
    APPLY_SWITCH, Launcher, PACKAGES_FOLDER, STAGING_SUFFIX, Version, app_folder,
    installed_versions,
};

use crate::platform::actions::panel_log;
use crate::platform::identity::product;
use crate::platform::paths;
use crate::platform::service::poll_thread::PollThread;
use crate::platform::settings::Settings;
use crate::setup::installation::EdgePlus;
use crate::setup::update_feed::{self, UpdateOffer};
use crate::setup::{install_layout, update_digest};

/// A newer release unpacked beside the running one, waiting for the user to
/// restart into it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) struct ReadyUpdate {
    pub version: String,
}

const THREAD_NAME: &str = "Update watch";
const NOT_FOUND: u16 = 404;
/// How often the loop wakes. It only goes to the network every `CHECK_EVERY`,
/// but a request for a check (the switch turned on) is seen within a tick.
const TICK: Duration = Duration::from_mins(1);
const CHECK_EVERY: Duration = Duration::from_hours(6);
/// After a failed check (no network at sign-in, GitHub down).
const RETRY_AFTER: Duration = Duration::from_mins(30);
const REQUEST_TIMEOUT: Duration = Duration::from_mins(10);
const USER_AGENT_PRODUCT: &str = "EdgePlus";
const DOWNLOAD_SUFFIX: &str = ".part";
const DOWNLOAD_CHUNK_BYTES: usize = 64 * 1024;

pub(crate) struct UpdateWatch {
    watcher: Arc<Watcher>,
    thread: Mutex<Option<PollThread>>,
}

impl UpdateWatch {
    /// `on_ready` runs on the watch thread.
    pub(crate) fn start(on_ready: impl Fn(ReadyUpdate) + Send + Sync + 'static) -> Self {
        let watcher = Arc::new(Watcher {
            on_ready: Box::new(on_ready),
            is_check_requested: AtomicBool::new(false),
            is_cancelled: AtomicBool::new(false),
            next_check: Mutex::new(None),
            pending: Mutex::new(None),
        });
        let first = Arc::clone(&watcher);
        let poll = Arc::clone(&watcher);
        let thread =
            PollThread::start(THREAD_NAME, TICK, move || first.poll(), move || poll.poll());
        Self {
            watcher,
            thread: Mutex::new(Some(thread)),
        }
    }

    /// Cancels a download in progress; later calls do nothing.
    pub(crate) fn stop(&self) {
        self.watcher.is_cancelled.store(true, Ordering::SeqCst);
        let thread = self
            .thread
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .take();
        if let Some(mut thread) = thread {
            thread.stop();
        }
    }

    /// The switch was turned on: look now rather than at the next check.
    pub(crate) fn check_soon(&self) {
        self.watcher
            .is_check_requested
            .store(true, Ordering::SeqCst);
    }

    #[must_use]
    pub(crate) fn pending(&self) -> Option<ReadyUpdate> {
        self.watcher
            .pending
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .as_ref()
            .map(|pending| pending.ready.clone())
    }
}

struct Pending {
    version: Version,
    ready: ReadyUpdate,
}

struct Watcher {
    on_ready: Box<dyn Fn(ReadyUpdate) + Send + Sync>,
    is_check_requested: AtomicBool,
    is_cancelled: AtomicBool,
    /// None until the first check, which is due at once.
    next_check: Mutex<Option<SystemTime>>,
    pending: Mutex<Option<Pending>>,
}

impl Watcher {
    fn poll(&self) {
        let next_check = *self
            .next_check
            .lock()
            .unwrap_or_else(PoisonError::into_inner);
        let is_due = self.is_check_requested.load(Ordering::SeqCst)
            || next_check.is_none_or(|next| SystemTime::now() >= next);
        if !is_due {
            return;
        }
        self.is_check_requested.store(false, Ordering::SeqCst);
        if !is_wanted() {
            self.schedule(CHECK_EVERY);
            return;
        }
        match self.look() {
            Ok(()) => self.schedule(CHECK_EVERY),
            Err(error) => {
                panel_log::write_error("update check failed", &error);
                self.schedule(RETRY_AFTER);
            }
        }
    }

    fn schedule(&self, delay: Duration) {
        *self
            .next_check
            .lock()
            .unwrap_or_else(PoisonError::into_inner) = Some(SystemTime::now() + delay);
    }

    fn look(&self) -> Result<(), ureq::Error> {
        let Ok(current) = Version::parse(product::VERSION) else {
            return Ok(());
        };
        let agent: ureq::Agent = ureq::Agent::config_builder()
            .timeout_global(Some(REQUEST_TIMEOUT))
            .user_agent(format!("{USER_AGENT_PRODUCT}/{}", product::VERSION))
            .build()
            .into();
        let feed_url = update_feed::url();
        let response = match agent.get(&feed_url).call() {
            Ok(response) => response,
            // The latest release carries no feed (it predates this install flow): nothing to offer.
            Err(ureq::Error::StatusCode(NOT_FOUND)) => return Ok(()),
            Err(error) => return Err(error),
        };
        let feed = response.into_body().read_to_string()?;
        let Some(offer) = update_feed::parse(&feed, &feed_url, current) else {
            return Ok(());
        };
        let is_new = self
            .pending
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .as_ref()
            .is_none_or(|pending| pending.version < offer.version);
        if !is_new {
            return Ok(());
        }
        self.stage(&agent, &offer)?;
        let ready = ReadyUpdate {
            version: offer.version.to_string(),
        };
        *self.pending.lock().unwrap_or_else(PoisonError::into_inner) = Some(Pending {
            version: offer.version,
            ready: ready.clone(),
        });
        (self.on_ready)(ready);
        Ok(())
    }

    /// Downloaded and checked against the feed's size and digest, unpacked into
    /// `app-x.y.z.staging` and renamed into place last, so a crash never leaves
    /// half a version.
    fn stage(&self, agent: &ureq::Agent, offer: &UpdateOffer) -> Result<(), ureq::Error> {
        let root = install_layout::root();
        let destination = app_folder(&root, offer.version);
        if destination.is_dir() {
            return Ok(());
        }
        let zip = self.download(agent, &root, offer)?;
        let staging = PathBuf::from(format!("{}{STAGING_SUFFIX}", destination.display()));
        let _ = fs::remove_dir_all(&staging);
        fs::create_dir_all(&staging)?;
        let archive = File::open(&zip)?;
        zip::ZipArchive::new(archive)
            .and_then(|mut archive| archive.extract(&staging))
            .map_err(io::Error::other)?;
        Launcher::new(EdgePlus, root).promote_staging(offer.version)?;
        Ok(())
    }

    fn download(
        &self,
        agent: &ureq::Agent,
        root: &Path,
        offer: &UpdateOffer,
    ) -> Result<PathBuf, ureq::Error> {
        let folder = root.join(PACKAGES_FOLDER);
        fs::create_dir_all(&folder)?;
        let target = folder.join(&offer.file);
        let partial = folder.join(format!("{}{DOWNLOAD_SUFFIX}", offer.file));
        let mut response = agent.get(&offer.download).call()?;
        {
            let mut source = response.body_mut().as_reader();
            let mut destination = File::create(&partial)?;
            self.copy_unless_cancelled(&mut source, &mut destination)?;
        }
        let is_whole = fs::metadata(&partial)?.len() == offer.size
            && update_digest::is_digest_of(&partial, &offer.sha256)?;
        if !is_whole {
            let _ = fs::remove_file(&partial);
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "the downloaded update does not match its size or digest",
            )
            .into());
        }
        fs::rename(&partial, &target)?;
        delete_other_downloads(&folder, &offer.file)?;
        Ok(target)
    }

    /// Checks between chunks, so stopping the watcher ends a download early.
    fn copy_unless_cancelled(
        &self,
        source: &mut impl Read,
        destination: &mut File,
    ) -> io::Result<()> {
        let mut chunk = vec![0_u8; DOWNLOAD_CHUNK_BYTES];
        loop {
            if self.is_cancelled.load(Ordering::SeqCst) {
                return Err(io::Error::new(
                    io::ErrorKind::Interrupted,
                    "the download was cancelled",
                ));
            }
            let read = source.read(&mut chunk)?;
            if read == 0 {
                return destination.flush();
            }
            let Some(received) = chunk.get(..read) else {
                return Err(io::Error::from(io::ErrorKind::InvalidData));
            };
            destination.write_all(received)?;
        }
    }
}

/// Fails when the folder cannot be listed; a file that cannot be deleted is
/// left for the next download.
fn delete_other_downloads(folder: &Path, keep: &str) -> io::Result<()> {
    let keep = paths::fold_case(keep);
    for entry in fs::read_dir(folder)?.flatten() {
        let is_file = entry.file_type().is_ok_and(|file_type| file_type.is_file());
        let is_other = paths::fold_case(&entry.file_name().to_string_lossy()) != keep;
        if is_file && is_other {
            let _ = fs::remove_file(entry.path());
        }
    }
    Ok(())
}

fn is_wanted() -> bool {
    Settings::load().auto_update && install_layout::is_installed_run()
}

/// Hands the replacing to the launcher (`--apply`): it waits for this process to
/// end, asks it to, then starts the newest version, and goes back to the one
/// before it when that does not start. The caller then ends this process.
pub(crate) fn restart_into(_update: &ReadyUpdate) -> io::Result<()> {
    let launcher = install_layout::launcher(&install_layout::root());
    let command = format!(
        "\"{}\" {APPLY_SWITCH} {}",
        launcher.display(),
        std::process::id()
    );
    if edge_native::start_detached(&command) {
        return Ok(());
    }
    Err(io::Error::other("the launcher could not be started"))
}

/// Removes the versions older than the running one and the zips already
/// unpacked; a folder still in use stays for the next run. The running version
/// has started, so the one before it is no longer needed for a rollback.
pub(crate) fn prune(running: Version) {
    let root = install_layout::root();
    for version in installed_versions(&root) {
        if version < running {
            let _ = fs::remove_dir_all(app_folder(&root, version));
        }
    }
    let _ = fs::remove_dir_all(root.join(PACKAGES_FOLDER));
}
