//! Runs whether or not the window is open. The window listens to it rather than
//! running watchers of its own, so there is one watcher of each and a boot
//! launch with no window is still the listener.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, OnceLock};

use crate::edge::edge_watch::EdgeWatch;
use crate::edge::version::edge_build::BuildChange;
use crate::platform::service::paused_toast;
use crate::platform::settings::Settings;
use crate::setup::update_watch::{ReadyUpdate, UpdateWatch};
use crate::themes::theme_library;
use crate::themes::theme_watch::{ThemeRefresh, ThemeWatch};

/// What the watchers saw, raised on a watcher's thread.
#[derive(Clone, Debug)]
pub enum ServiceEvent {
    BuildChanged,
    /// An Edge browser process started (once it got the DLL, with auto-inject
    /// on) or closed: the window re-reads its state.
    BrowsersChanged,
    ThemeRefreshed(ThemeRefresh),
    /// A newer release is downloaded and checked.
    UpdateReady,
    /// The paused-features toast was clicked: open the window.
    ToastClicked,
}

type Listener = Arc<dyn Fn(ServiceEvent) + Send + Sync>;

struct Service {
    edge: EdgeWatch,
    themes: ThemeWatch,
    updates: UpdateWatch,
}

static SERVICE: OnceLock<Service> = OnceLock::new();
static LISTENER: OnceLock<Listener> = OnceLock::new();
/// A toast is only for when no window is open.
static IS_WINDOW_OPEN: AtomicBool = AtomicBool::new(false);

fn raise(event: ServiceEvent) {
    if let Some(listener) = LISTENER.get() {
        listener(event);
    }
}

/// Once per process; `listener` hears every event.
pub fn start(listener: impl Fn(ServiceEvent) + Send + Sync + 'static) {
    let _ = LISTENER.set(Arc::new(listener));
    theme_library::register_all(&Settings::load().theming);
    SERVICE.get_or_init(|| Service {
        edge: EdgeWatch::start(
            |change| on_build_changed(&change),
            || raise(ServiceEvent::BrowsersChanged),
        ),
        themes: ThemeWatch::start(|refresh| raise(ServiceEvent::ThemeRefreshed(refresh))),
        updates: UpdateWatch::start(|_ready| raise(ServiceEvent::UpdateReady)),
    });
}

fn on_build_changed(change: &BuildChange) {
    if !IS_WINDOW_OPEN.load(Ordering::SeqCst) {
        paused_toast::show_once_per_build(change, || raise(ServiceEvent::ToastClicked));
    }
    raise(ServiceEvent::BuildChanged);
}

pub fn set_window_open(is_open: bool) {
    IS_WINDOW_OPEN.store(is_open, Ordering::SeqCst);
}

/// The switch was turned on: look now rather than at the next scheduled check.
pub fn check_for_update_soon() {
    if let Some(service) = SERVICE.get() {
        service.updates.check_soon();
    }
}

/// The downloaded release waiting for a restart, if any.
#[must_use]
pub fn pending_update() -> Option<ReadyUpdate> {
    SERVICE.get().and_then(|service| service.updates.pending())
}

/// Stops every watcher before the process ends, so none is cut off mid-write.
pub fn stop() {
    if let Some(service) = SERVICE.get() {
        service.edge.stop();
        service.themes.stop();
        service.updates.stop();
    }
}
