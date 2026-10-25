//! The DLL holds the unload event open for its whole lifetime, so the event
//! existing is exactly "the DLL is loaded right now".

#[must_use]
pub fn is_loaded() -> bool {
    todo!()
}

pub fn signal_config_changed() {
    todo!()
}
