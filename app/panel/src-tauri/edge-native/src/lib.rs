//! Win32 glue for the Fix Edge panel.
//!
//! This is the one crate allowed `unsafe`; the panel crate forbids it. Each
//! module owns one concern:
//!
//! * [`process`] - find Edge's browser processes (the ones that own a window).
//! * [`inject`] - load / unload the native-split DLL in those processes, live.
//! * [`protocol`] - the DLL's config channel: the wire struct, the config file
//!   and the named events, mirroring `app/native-split/src/{config.h,dllmain.cpp}`.
//! * [`registry`] - add / remove a theme through Edge's external-extension loader.
//! * [`crx`] - read a local theme `.crx`'s extension id and version.
//! * [`pack`] - pack a theme folder or `.zip` into a signed `.crx`.
//! * [`devmode`] - wait for Edge to exit, then snooze its developer-mode prompt.
//! * [`version`] - read a binary's file version, as the DLL's self-check does.

mod crx;
mod devmode;
mod inject;
mod manifest;
mod pack;
mod process;
mod protocol;
mod registry;
mod version;

pub use crx::{read_crx, CrxInfo};
pub use devmode::{default_user_data, wait_for_edge_exit_then_snooze};
pub use inject::{free_module, inject_into, is_split_loaded, signal_unload};
pub use pack::{pack_theme, PackedTheme};
pub use process::{
    edge_browser_pids, edge_pids_missing_module, edge_pids_with_module, module_path,
};
pub use protocol::{signal_config_changed, write_config};
pub use registry::{install_local, install_store, uninstall, Store};
pub use version::{file_version, FileVersion, ParseVersionError};
