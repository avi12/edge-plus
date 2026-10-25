//! Every Win32 and `WinRT` call the Edge+ core makes.
//!
//! This is the one crate allowed `unsafe`; the core forbids it. Each module
//! owns one concern:
//!
//! * [`process`] - Edge's browser processes (the ones that own a window), the
//!   modules loaded in them, and waiting for a process to exit.
//! * [`inject`] - `LoadLibraryW` / `FreeLibrary` on a remote thread.
//! * [`sync`] - named events (the DLL's channel) and named mutexes.
//! * [`version`] - a binary's file version, as the DLL's self-check reads it.
//! * [`registry`] - the per-user and machine registry values Edge+ reads and
//!   writes.
//! * [`rsa`] - the CNG RSA keys local themes are signed with.
//! * [`spawn`] - starting a process detached from any job.
//! * [`toast`] - the paused-features toast.
//! * [`memory`] - trimming the windowless listener's working set.
//! * [`clock`] - local time, for the log and the window's status line.

mod clock;
mod inject;
mod memory;
mod process;
mod registry;
mod rsa;
mod spawn;
mod sync;
mod toast;
mod version;
// Shared by the modules above: owned handles and UTF-16 strings.
mod handle;
mod wide;

pub use clock::{local_time_of_day, local_timestamp};
pub use inject::{free_module, inject_into};
pub use memory::trim_working_set;
pub use process::{
    browser_pids, module_base, module_path, pids_missing_module, pids_with_module, wait_for_exit,
};
pub use registry::{Hive, delete_tree, delete_value, read_binary, read_string, write_string};
pub use rsa::RsaKey;
pub use spawn::start_detached;
pub use sync::{NamedMutex, is_event_open, signal_event};
pub use toast::{register_toast_app, show_toast};
pub use version::file_version;
