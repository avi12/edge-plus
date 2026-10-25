//! The window's process: Node running ui/main.ts, which draws the WinUI window.
//! The core starts it on demand and talks to it over its stdin and stdout, one
//! JSON message per line (protocol.rs). The window renders the View it is sent
//! and asks for Actions; it never touches settings, Edge or the DLL.
//!
//! In a dev run the window's TypeScript is watched, and an edit swaps a fresh
//! window in where the old one was (the panel's version of the split DLL's hot
//! reload).

use std::io::{BufRead, BufReader, Write};
use std::os::windows::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::{Mutex, MutexGuard, OnceLock, PoisonError};
use std::thread;
use std::time::{Duration, SystemTime};

use crate::platform::action_error::{ActionError, ActionResult};
use crate::platform::background_service::{self, ServiceEvent};
use crate::platform::panel_actions;
use crate::platform::panel_log;
use crate::ui::inject_offer::{self, InjectOffer};
use crate::ui::panel_layout::{CardSlot, OwnCard};
use crate::ui::panel_state::PanelState;
use crate::ui::protocol::{Action, Bounds, CoreMessage, UiMessage};
use crate::ui::{view, wording};

/// Keeps Node's console from flashing up.
const CREATE_NO_WINDOW: u32 = 0x0800_0000;
const NODE: &str = "node";
const NODE_FLAGS: [&str; 1] = ["--disable-warning=ExperimentalWarning"];
const ENTRY: &str = "main.ts";
const WINDOW_SOURCE_EXTENSION: &str = "ts";
const DEV_POLL: Duration = Duration::from_secs(1);
/// An editor saves in several writes; a swap waits for them to settle.
const DEV_SETTLE: Duration = Duration::from_millis(400);

/// How the core decides what to do once the last window closes.
pub type OnLastClose = fn();

struct Session {
    id: u64,
    stdin: ChildStdin,
    /// What the window showed last; the Inject card's next state depends on it.
    offer: InjectOffer,
    bounds: Option<Bounds>,
}

struct Host {
    session: Mutex<Option<Session>>,
    next_id: Mutex<u64>,
    on_last_close: OnLastClose,
}

static HOST: OnceLock<Host> = OnceLock::new();

fn host() -> &'static Host {
    HOST.get().expect("window_host::init runs first")
}

fn lock<T>(mutex: &Mutex<T>) -> MutexGuard<'_, T> {
    mutex.lock().unwrap_or_else(PoisonError::into_inner)
}

/// Once, at start. In a dev run it also starts watching the window's sources.
pub fn init(on_last_close: OnLastClose) {
    let _ = HOST.set(Host {
        session: Mutex::new(None),
        next_id: Mutex::new(0),
        on_last_close,
    });
    if let Some(sources) = dev_sources() {
        thread::Builder::new()
            .name("window sources".to_owned())
            .spawn(move || watch_sources(&sources))
            .ok();
    }
}

#[must_use]
pub fn is_open() -> bool {
    lock(&host().session).is_some()
}

/// Opens the window, or brings back the one already open.
pub fn show() {
    if is_open() {
        send(&CoreMessage::BringBack);
        return;
    }
    open(None, None);
}

/// Ends the window, if one is open.
pub fn close() {
    send(&CoreMessage::Close);
}

/// The watchers' news, for the window while one is open.
pub fn on_service_event(event: &ServiceEvent) {
    match event {
        ServiceEvent::ToastClicked => show(),
        ServiceEvent::ThemeRefreshed(refresh) => send(&CoreMessage::Notice {
            text: refresh.error.clone().unwrap_or_default(),
        }),
        ServiceEvent::BuildChanged(_)
        | ServiceEvent::BrowsersChanged
        | ServiceEvent::UpdateReady(_) => refresh(),
    }
}

fn open(placement: Option<Bounds>, status: Option<String>) {
    let Some((program, arguments)) = window_command() else {
        panel_log::write("the window's scripts are missing; no window opened");
        return;
    };
    let spawned = Command::new(&program)
        .args(&arguments)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .creation_flags(CREATE_NO_WINDOW)
        .spawn();
    let mut child = match spawned {
        Ok(child) => child,
        Err(error) => {
            panel_log::write_error("starting the window failed", &error);
            return;
        }
    };
    let (Some(stdin), Some(stdout), Some(stderr)) =
        (child.stdin.take(), child.stdout.take(), child.stderr.take())
    else {
        return;
    };
    let id = {
        let mut next_id = lock(&host().next_id);
        *next_id += 1;
        *next_id
    };
    let replaced = lock(&host().session).replace(Session {
        id,
        stdin,
        offer: InjectOffer::Hidden,
        bounds: placement,
    });
    background_service::set_window_open(true);
    if let Some(bounds) = placement {
        send(&CoreMessage::Place { bounds });
    }
    send_view(&panel_actions::load());
    if let Some(line) = status {
        send(&CoreMessage::Status { line });
    }
    if let Some(mut old) = replaced {
        write_line(&mut old.stdin, &CoreMessage::Close);
    }
    thread::Builder::new()
        .name("window stderr".to_owned())
        .spawn(move || log_stderr(stderr))
        .ok();
    thread::Builder::new()
        .name("window link".to_owned())
        .spawn(move || listen(id, stdout, child))
        .ok();
}

fn log_stderr(stderr: impl std::io::Read) {
    for line in BufReader::new(stderr).lines().map_while(Result::ok) {
        panel_log::write(&format!("window: {line}"));
    }
}

/// Until the window's process ends; then, if it was still the window shown,
/// the core decides whether to keep running.
fn listen(id: u64, stdout: impl std::io::Read, mut child: Child) {
    for line in BufReader::new(stdout).lines().map_while(Result::ok) {
        match serde_json::from_str::<UiMessage>(&line) {
            Ok(message) => handle(id, message),
            Err(error) => panel_log::write_error("the window sent an unreadable message", &error),
        }
    }
    let _ = child.wait();
    let is_current = {
        let mut session = lock(&host().session);
        let is_current = session.as_ref().is_some_and(|open| open.id == id);
        if is_current {
            *session = None;
        }
        is_current
    };
    if is_current {
        background_service::set_window_open(false);
        (host().on_last_close)();
    }
}

fn handle(id: u64, message: UiMessage) {
    match message {
        UiMessage::Refresh => refresh(),
        UiMessage::Bounds { bounds } => {
            if let Some(session) = lock(&host().session).as_mut().filter(|open| open.id == id) {
                session.bounds = Some(bounds);
            }
        }
        UiMessage::Action { id: request, action } => {
            thread::Builder::new()
                .name("window action".to_owned())
                .spawn(move || run(request, action))
                .ok();
        }
    }
}

/// Off the link's thread: an injection can wait on Edge.
fn run(request: u32, action: Action) {
    if action == Action::RestartToUpdate {
        restart_to_update(request);
        return;
    }
    let error = match perform(action) {
        Ok(state) => {
            send_view(&state);
            None
        }
        Err(error) => Some(shown(&error)),
    };
    send(&CoreMessage::Reply { id: request, error });
}

/// Only a User error carries a sentence for the window; anything else is
/// logged and said plainly.
fn shown(error: &ActionError) -> String {
    match error {
        ActionError::User(message) => message.clone(),
        ActionError::Internal(message) => {
            panel_log::write(&format!("panel action failed: {message}"));
            wording::ACTION_FAILED.to_owned()
        }
    }
}

fn perform(action: Action) -> ActionResult<PanelState> {
    match action {
        Action::SetSwitch { key, is_on } => match view::slot_of(key) {
            Some(CardSlot::Feature { feature, .. }) => panel_actions::set_feature(feature, is_on),
            Some(CardSlot::Own(own)) => set_own(own, is_on),
            None => Err(ActionError::Internal(format!("no card for {key:?}"))),
        },
        Action::Inject => panel_actions::inject(),
        Action::AddStoreTheme { url } => panel_actions::add_store_theme(&url),
        Action::AddLocalTheme { path } => panel_actions::add_local_theme(Path::new(&path)),
        Action::RemoveTheme { id } => panel_actions::remove_theme(&id),
        Action::ReorderThemes { ids } => panel_actions::reorder_themes(&ids),
        Action::AssignTheme { mode, id } => panel_actions::assign_theme(mode, &id),
        Action::RestartToUpdate => Err(ActionError::Internal(
            "restart to update is not a state change".to_owned(),
        )),
    }
}

fn set_own(own: OwnCard, is_on: bool) -> ActionResult<PanelState> {
    match own {
        OwnCard::StartWithWindows => panel_actions::set_start_with_windows(is_on),
        OwnCard::AutoInject => panel_actions::set_auto_inject(is_on),
        OwnCard::AutoUpdate => panel_actions::set_auto_update(is_on),
    }
}

/// The new exe waits for this process to end, which is done here once the
/// swap succeeded.
fn restart_to_update(request: u32) {
    match panel_actions::restart_to_update() {
        Ok(()) => {
            background_service::stop();
            std::process::exit(0);
        }
        Err(error) => {
            panel_log::write(&format!("restarting into the update failed: {error}"));
            send(&CoreMessage::Reply {
                id: request,
                error: Some(wording::UPDATE_FAILED.to_owned()),
            });
        }
    }
}

fn refresh() {
    if is_open() {
        send_view(&panel_actions::load());
    }
}

fn send_view(state: &PanelState) {
    let message = {
        let mut session = lock(&host().session);
        let Some(open) = session.as_mut() else {
            return;
        };
        open.offer = inject_offer::next(open.offer, state);
        CoreMessage::View {
            view: Box::new(view::build(
                state,
                open.offer,
                background_service::pending_update().as_ref(),
            )),
        }
    };
    send(&message);
}

fn send(message: &CoreMessage) {
    if let Some(open) = lock(&host().session).as_mut() {
        write_line(&mut open.stdin, message);
    }
}

fn write_line(stdin: &mut ChildStdin, message: &CoreMessage) {
    let Ok(mut line) = serde_json::to_string(message) else {
        return;
    };
    line.push('\n');
    let _ = stdin.write_all(line.as_bytes()).and_then(|()| stdin.flush());
}

/// The window's scripts in this checkout, when this exe was built from one.
fn dev_sources() -> Option<PathBuf> {
    let sources = Path::new(env!("CARGO_MANIFEST_DIR")).join("..").join("ui");
    let executable = std::env::current_exe().ok()?;
    let checkout = Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
    let is_dev_run = sources.join(ENTRY).exists()
        && executable.starts_with(checkout.canonicalize().ok()?.as_path());
    is_dev_run.then(|| sources.canonicalize().unwrap_or(sources))
}

fn window_command() -> Option<(PathBuf, Vec<String>)> {
    let entry = dev_sources()?.join(ENTRY);
    let mut arguments: Vec<String> = NODE_FLAGS.iter().map(|&flag| flag.to_owned()).collect();
    arguments.push(entry.to_string_lossy().into_owned());
    Some((PathBuf::from(NODE), arguments))
}

fn watch_sources(sources: &Path) {
    let mut known = newest_change(sources);
    loop {
        thread::sleep(DEV_POLL);
        let latest = newest_change(sources);
        if latest == known {
            continue;
        }
        thread::sleep(DEV_SETTLE);
        known = newest_change(sources);
        if is_open() {
            swap();
        }
    }
}

/// A fresh window where the old one was; the old one closes once the new one
/// is up.
fn swap() {
    let placement = lock(&host().session).as_ref().and_then(|open| open.bounds);
    let line = format!("Panel updated {}", edge_native::local_time_of_day());
    open(placement, Some(line));
}

fn newest_change(folder: &Path) -> Option<SystemTime> {
    let mut newest: Option<SystemTime> = None;
    let entries = std::fs::read_dir(folder).ok()?;
    for entry in entries.flatten() {
        let path = entry.path();
        let changed = if path.is_dir() {
            newest_change(&path)
        } else {
            let is_source = path
                .extension()
                .is_some_and(|extension| extension == WINDOW_SOURCE_EXTENSION);
            is_source
                .then(|| entry.metadata().and_then(|metadata| metadata.modified()).ok())
                .flatten()
        };
        newest = newest.max(changed);
    }
    newest
}
