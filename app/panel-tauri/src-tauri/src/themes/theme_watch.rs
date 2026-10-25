//! A local theme's folder or file is watched, and an edit re-installed once it
//! settles (the same new fingerprint on two polls in a row).

/// `error` is None when the edit was applied.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ThemeRefresh {
    pub id: String,
    pub error: Option<String>,
}

pub struct ThemeWatch {}

impl ThemeWatch {
    /// `on_refreshed` runs on the watch thread.
    pub fn start(on_refreshed: impl Fn(ThemeRefresh) + Send + Sync + 'static) -> Self {
        let _ = on_refreshed;
        todo!()
    }

    /// Waits for a poll in progress; later calls do nothing.
    pub fn stop(&self) {
        todo!()
    }
}
