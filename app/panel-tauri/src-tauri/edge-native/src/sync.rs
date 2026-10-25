//! Named events and mutexes.

/// Whether the named event exists (OpenEventW succeeds).
#[must_use]
pub fn is_event_open(name: &str) -> bool {
    let _ = name;
    todo!()
}

/// Sets the named event; a no-op when it does not exist.
pub fn signal_event(name: &str) {
    let _ = name;
    todo!()
}

/// Held until dropped.
pub struct NamedMutex {}

impl NamedMutex {
    /// None when another process already holds one of this name.
    #[must_use]
    pub fn acquire_sole(name: &str) -> Option<Self> {
        let _ = name;
        todo!()
    }
}
