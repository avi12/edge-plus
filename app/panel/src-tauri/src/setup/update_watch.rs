//! Asks the repository's update manifest (`update_feed`) for a newer release at
//! start and then every six hours, and downloads it in the background so the
//! window can offer a restart at once. Only the installed package updates: a
//! build run from a checkout is never touched. Off (`Settings::auto_update`),
//! Edge+ never contacts GitHub.

use std::fs::{self, File};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex, PoisonError};
use std::time::{Duration, SystemTime};

use crate::platform::actions::panel_log;
use crate::platform::identity::product;
use crate::platform::paths;
use crate::platform::service::poll_thread::PollThread;
use crate::platform::settings::Settings;
use crate::setup::update_feed::{self, UpdateAsset, Version};
use crate::setup::{package_install, update_digest};

/// A newer release whose file is downloaded and checked, waiting for the user
/// to restart into it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ReadyUpdate {
    pub version: String,
    pub file: PathBuf,
    /// The manifest's digest of `file`, checked again before it is used, since
    /// the download may have been altered since.
    pub sha256: String,
}

const THREAD_NAME: &str = "Update watch";
/// How often the loop wakes. It only goes to the network every `CHECK_EVERY`,
/// but a request for a check (the switch turned on) is seen within a tick.
const TICK: Duration = Duration::from_mins(1);
const CHECK_EVERY: Duration = Duration::from_hours(6);
/// After a failed check (no network at sign-in, GitHub down).
const RETRY_AFTER: Duration = Duration::from_mins(30);
const REQUEST_TIMEOUT: Duration = Duration::from_mins(10);
const USER_AGENT_PRODUCT: &str = "EdgePlus";
const DOWNLOAD_PREFIX: &str = "EdgePlus-";
const DOWNLOAD_SUFFIX: &str = ".part";
const PACKAGE_EXTENSION: &str = ".msix";
const DOWNLOAD_CHUNK_BYTES: usize = 64 * 1024;

pub struct UpdateWatch {
    watcher: Arc<Watcher>,
    thread: Mutex<Option<PollThread>>,
}

impl UpdateWatch {
    /// `on_ready` runs on the watch thread.
    pub fn start(on_ready: impl Fn(ReadyUpdate) + Send + Sync + 'static) -> Self {
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
    pub fn stop(&self) {
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
    pub fn check_soon(&self) {
        self.watcher
            .is_check_requested
            .store(true, Ordering::SeqCst);
    }

    #[must_use]
    pub fn pending(&self) -> Option<ReadyUpdate> {
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
        let Some(current) = Version::parse(product::VERSION) else {
            return Ok(());
        };
        let agent: ureq::Agent = ureq::Agent::config_builder()
            .timeout_global(Some(REQUEST_TIMEOUT))
            .user_agent(format!("{USER_AGENT_PRODUCT}/{}", product::VERSION))
            .build()
            .into();
        let manifest = agent
            .get(update_feed::url())
            .call()?
            .body_mut()
            .read_to_string()?;
        let Some(offer) = update_feed::parse(&manifest, current) else {
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
        let file_name = format!("{DOWNLOAD_PREFIX}{}{PACKAGE_EXTENSION}", offer.version);
        let file = self.download(&agent, &offer.package, &file_name)?;
        let ready = ReadyUpdate {
            version: offer.version.to_string(),
            file,
            sha256: offer.package.sha256.clone(),
        };
        *self.pending.lock().unwrap_or_else(PoisonError::into_inner) = Some(Pending {
            version: offer.version,
            ready: ready.clone(),
        });
        (self.on_ready)(ready);
        Ok(())
    }

    /// Checked against the manifest's digest before it is kept; a download from
    /// an earlier run that still matches is used as it is.
    fn download(
        &self,
        agent: &ureq::Agent,
        asset: &UpdateAsset,
        file_name: &str,
    ) -> Result<PathBuf, ureq::Error> {
        let folder = paths::update_folder();
        fs::create_dir_all(&folder)?;
        let target = folder.join(file_name);
        let is_already_downloaded =
            target.is_file() && update_digest::is_digest_of(&target, &asset.sha256)?;
        if is_already_downloaded {
            return Ok(target);
        }
        let partial = folder.join(format!("{file_name}{DOWNLOAD_SUFFIX}"));
        let mut response = agent.get(&asset.download).call()?;
        {
            let mut source = response.body_mut().as_reader();
            let mut destination = File::create(&partial)?;
            self.copy_unless_cancelled(&mut source, &mut destination)?;
        }
        if !update_digest::is_digest_of(&partial, &asset.sha256)? {
            let _ = fs::remove_file(&partial);
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "the downloaded update does not match its digest",
            )
            .into());
        }
        fs::rename(&partial, &target)?;
        delete_other_downloads(&folder, file_name)?;
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
            destination.write_all(&chunk[..read])?;
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
    Settings::load().auto_update && package_install::is_packaged()
}

/// Hands the package to a hidden PowerShell that installs it once this process
/// exits and starts the new app; the caller then ends this process.
pub fn restart_into(update: &ReadyUpdate) -> io::Result<()> {
    // Checked again here, since the download may have been altered since.
    if !update_digest::is_digest_of(&update.file, &update.sha256)? {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "the downloaded package does not match its digest",
        ));
    }
    let Some(app_user_model_id) = package_install::app_user_model_id() else {
        return Err(io::Error::other("this copy is not the installed package"));
    };
    let script =
        package_install::update_script(std::process::id(), &update.file, &app_user_model_id);
    let is_started = edge_native::start_detached(&package_install::hidden_powershell(&script));
    if !is_started {
        return Err(io::Error::other("the installer could not be started"));
    }
    Ok(())
}
