//! The DLL holds the unload event open for its whole lifetime, so the event
//! existing is exactly "the DLL is loaded right now": the core never tracks
//! per-process state. Both event names are the DLL's own (dllmain.cpp).

use std::io;

use crate::dll::config_wire::{self, DllConfig};
use crate::platform::{paths, shared};

/// Atomic, so the DLL never reads half a file.
pub fn write_config(config: &DllConfig) -> io::Result<()> {
    paths::write_atomically(&paths::config_file(), &config_wire::encode(config))
}

#[must_use]
pub fn is_loaded() -> bool {
    edge_native::is_event_open(shared::UNLOAD_EVENT)
}

/// Both signals are no-ops when no DLL is loaded.
pub fn signal_unload() {
    edge_native::signal_event(shared::UNLOAD_EVENT);
}

pub fn signal_config_changed() {
    edge_native::signal_event(shared::CONFIG_EVENT);
}
