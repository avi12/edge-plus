//! Every Win32 and `WinRT` call the Edge+ core makes.
//!
//! This is the one crate allowed `unsafe`; the core forbids it. Each module
//! owns one concern, grouped by what it touches:
//!
//! * [`process`] - other processes and our own:
//!   * [`process::browsers`] - Edge's browser processes (the ones that own a
//!     window), the modules loaded in them, and waiting for a process to exit.
//!   * [`process::inject`] - `LoadLibraryW` / `FreeLibrary` on a remote thread.
//!   * [`process::spawn`] - starting a process detached from any job.
//!   * [`process::memory`] - trimming the windowless listener's working set.
//! * [`kernel`] - kernel objects and primitives:
//!   * [`kernel::sync`] - named events (the DLL's channel) and named mutexes.
//!   * [`kernel::version`] - a binary's file version, as the DLL's self-check
//!     reads it.
//!   * [`kernel::clock`] - local time, for the log and the window's status
//!     line.
//!   * [`kernel::handle`] and [`kernel::wide`] - owned handles and UTF-16
//!     strings, shared by every module.
//! * [`system`] - Windows' own stores and services:
//!   * [`system::registry`] - the per-user and machine registry values Edge+
//!     reads and writes.
//!   * [`system::rsa`] - the CNG RSA keys local themes are signed with.
//!   * [`system::toast`] - the paused-features toast.
//!   * [`system::packages`] - installing MSIX packages for the current user.

mod kernel;
mod process;
mod system;

pub use kernel::clock::{local_time_of_day, local_timestamp};
pub use kernel::sync::{NamedMutex, is_event_open, signal_event};
pub use kernel::version::file_version;
pub use process::browsers::{
    browser_pids, module_base, module_path, pids_missing_module, pids_with_module, wait_for_exit,
};
pub use process::inject::{free_module, inject_into};
pub use process::memory::trim_working_set;
pub use process::spawn::start_detached;
pub use system::packages::install_packages;
pub use system::registry::{
    Hive, delete_tree, delete_value, read_binary, read_string, write_string,
};
pub use system::rsa::RsaKey;
pub use system::toast::{register_toast_app, show_toast};
