//! Asks the repository's update manifest (`update_feed`) for a newer release at
//! start and then every six hours, and downloads it in the background so the
//! window can offer a restart at once. Only a published exe replaces itself: a
//! build run from a checkout is never touched. Off (`Settings::auto_update`),
//! Edge+ never contacts GitHub.

use std::fs::{self, File};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex, PoisonError};
use std::time::{Duration, SystemTime};

use crate::platform::poll_thread::PollThread;
use crate::platform::settings::Settings;
use crate::platform::{panel_log, paths, product, switches};
use crate::setup::update_feed::{self, UpdateAsset, Version};
use crate::setup::{msi_install, update_install};

/// A newer release whose file is downloaded and checked, waiting for the user
/// to restart into it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ReadyUpdate {
    pub version: String,
    pub file: PathBuf,
    /// The MSI, for a copy the MSI installed; else the portable exe.
    pub is_installer: bool,
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
const EXE_EXTENSION: &str = ".exe";
const INSTALLER_EXTENSION: &str = ".msi";
/// cargo's output folder inside the crate, unless `CARGO_TARGET_DIR` moved it.
const CARGO_TARGET_FOLDER: &str = "target";
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
        // A release without an MSI is not offered to a copy the MSI installed.
        let is_installer = msi_install::is_installed_at(&std::env::current_exe()?);
        let asset = if is_installer {
            offer.installer.as_ref()
        } else {
            Some(&offer.exe)
        };
        let Some(asset) = asset else {
            return Ok(());
        };
        let extension = if is_installer {
            INSTALLER_EXTENSION
        } else {
            EXE_EXTENSION
        };
        let file_name = format!("{DOWNLOAD_PREFIX}{}{extension}", offer.version);
        let file = self.download(&agent, asset, &file_name)?;
        let ready = ReadyUpdate {
            version: offer.version.to_string(),
            file,
            is_installer,
            sha256: asset.sha256.clone(),
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
            target.is_file() && update_install::is_digest_of(&target, &asset.sha256)?;
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
        if !update_install::is_digest_of(&partial, &asset.sha256)? {
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
    Settings::load().auto_update && is_published_exe()
}

/// A build from a checkout runs from cargo's output folder in that checkout
/// (`cargo run`, `tauri dev`, a local release build); a published exe was
/// copied out of it into a release and runs from anywhere else. The C#
/// panel told them apart by the host assembly a checkout build has beside it;
/// a Rust exe has no such file, so the folder it runs from decides. Both paths
/// are canonical, so case and `..` cannot hide one inside the other.
fn is_published_exe() -> bool {
    let Ok(executable) = std::env::current_exe().and_then(fs::canonicalize) else {
        return false;
    };
    !checkout_target_folders()
        .iter()
        .filter_map(|folder| fs::canonicalize(folder).ok())
        .any(|folder| executable.starts_with(folder))
}

/// Where cargo put this build's exe when it was built from the checkout.
fn checkout_target_folders() -> Vec<PathBuf> {
    let mut folders = vec![Path::new(env!("CARGO_MANIFEST_DIR")).join(CARGO_TARGET_FOLDER)];
    if let Some(moved) = option_env!("CARGO_TARGET_DIR") {
        folders.push(PathBuf::from(moved));
    }
    folders
}

/// Puts the pending update in place and starts it (the portable exe), or hands
/// the MSI to a hidden PowerShell that runs it once this process exits; the
/// caller then ends this process. The new exe swaps its own DLL into Edge, so
/// no Edge restart; the MSI's update starts the exe again itself.
pub fn restart_into(update: &ReadyUpdate) -> io::Result<()> {
    let running = std::env::current_exe()?;
    if update.is_installer {
        return run_installer_after_exit(update, &running);
    }
    update_install::install(&update.file, &running, &update.sha256, SystemTime::now())?;
    Command::new(&running)
        .args([switches::RESTARTED_FROM, &std::process::id().to_string()])
        .spawn()?;
    Ok(())
}

/// Checked again here, since the download may have been altered since.
fn run_installer_after_exit(update: &ReadyUpdate, running: &Path) -> io::Result<()> {
    if !update_install::is_digest_of(&update.file, &update.sha256)? {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "the downloaded installer does not match its digest",
        ));
    }
    let arguments = msi_install::installer_arguments(
        &update.file,
        has_shortcut(&common_desktop()),
        has_shortcut(&common_programs()),
    );
    let script = msi_install::update_script(std::process::id(), &arguments, running);
    let is_started = edge_native::start_detached(&msi_install::hidden_powershell(&script));
    if !is_started {
        return Err(io::Error::other("the installer could not be started"));
    }
    Ok(())
}

fn has_shortcut(folder: &Path) -> bool {
    msi_install::shortcut_file(folder, product::NAME).is_file()
}

/// The all-users desktop (.NET's `CommonDesktopDirectory`) the installer's
/// shortcut goes on, at its default place: edge-native has no known-folder
/// lookup.
fn common_desktop() -> PathBuf {
    environment_folder("PUBLIC").join("Desktop")
}

/// The all-users Start menu programs (`CommonPrograms`), at its default place.
fn common_programs() -> PathBuf {
    environment_folder("ProgramData")
        .join("Microsoft")
        .join("Windows")
        .join("Start Menu")
        .join("Programs")
}

fn environment_folder(variable: &str) -> PathBuf {
    std::env::var_os(variable).map_or_else(PathBuf::new, PathBuf::from)
}
