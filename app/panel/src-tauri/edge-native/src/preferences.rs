//! Editing each Edge profile's `Preferences` once Edge has exited.
//!
//! Edge rewrites `Preferences` from memory while it runs, so an edit only
//! sticks once Edge has fully exited - which is also when Edge next reads it.
//! The panel re-runs itself headless with a switch to do the waiting, so it
//! survives the panel window closing; what to write is the caller's (the
//! panel's `edge_prefs`), this module only waits, finds the profiles and swaps
//! each file in whole.

use std::os::windows::ffi::OsStrExt;
use std::path::{Path, PathBuf};
use std::thread::sleep;
use std::time::Duration;

use windows_sys::Win32::Foundation::{
    CloseHandle, GetLastError, ERROR_ALREADY_EXISTS, INVALID_HANDLE_VALUE,
};
use windows_sys::Win32::Storage::FileSystem::{CreateFileW, OPEN_EXISTING};
use windows_sys::Win32::System::SystemInformation::GetSystemTimeAsFileTime;
use windows_sys::Win32::System::Threading::CreateMutexW;

// One waiter at a time (every armed panel starts one).
const WAITER_MUTEX: &str = r"Local\fix-edge.preferences-waiter";
const POLL: Duration = Duration::from_secs(2);
const GENERIC_READ: u32 = 0x8000_0000;

/// Edge's default user-data directory, where its profiles live.
#[must_use]
pub fn default_user_data() -> PathBuf {
    let local = std::env::var_os("LOCALAPPDATA").map_or_else(|| PathBuf::from("."), PathBuf::from);
    local.join("Microsoft").join("Edge").join("User Data")
}

/// Wait until the Edge at `user_data` has fully exited. Returns false at once,
/// without waiting, when another waiter already holds the single-waiter mutex:
/// that one reads the settings when Edge exits, so it applies this change too.
#[must_use]
pub fn wait_for_edge_exit(user_data: &Path) -> bool {
    if !claim_sole_waiter() {
        return false;
    }
    while is_in_use(user_data) {
        sleep(POLL);
    }
    true
}

/// Apply `edit` to the parsed `Preferences` of every profile of `user_data`,
/// writing each file back whole. Call only while Edge is not running.
pub fn edit_every_profile(user_data: &Path, edit: impl Fn(&mut serde_json::Value)) {
    for profile in profile_directories(user_data) {
        edit_preferences(&user_data.join(&profile).join("Preferences"), &edit);
    }
}

/// Now, the way Edge stamps its preferences: microseconds since 1601.
#[must_use]
pub fn now_microseconds_since_1601() -> u64 {
    let mut filetime = windows_sys::Win32::Foundation::FILETIME {
        dwLowDateTime: 0,
        dwHighDateTime: 0,
    };
    // SAFETY: writes the current time into the local FILETIME.
    unsafe {
        GetSystemTimeAsFileTime(std::ptr::addr_of_mut!(filetime));
    }
    let hundred_nanoseconds =
        (u64::from(filetime.dwHighDateTime) << 32) | u64::from(filetime.dwLowDateTime);
    hundred_nanoseconds / 10
}

fn claim_sole_waiter() -> bool {
    let name: Vec<u16> = WAITER_MUTEX.encode_utf16().chain([0]).collect();
    // SAFETY: the name is NUL-terminated; the handle is intentionally leaked for
    // the life of the waiter process so the mutex is held until it exits.
    unsafe {
        let handle = CreateMutexW(std::ptr::null(), 1, name.as_ptr());
        handle.is_null() || GetLastError() != ERROR_ALREADY_EXISTS
    }
}

// Edge holds User Data\lockfile open, unshared, for its whole run - so failing
// to open it for exclusive read means Edge is still up.
fn is_in_use(user_data: &Path) -> bool {
    let lockfile = user_data.join("lockfile");
    if !lockfile.exists() {
        return false;
    }
    let wide: Vec<u16> = lockfile.as_os_str().encode_wide().chain([0]).collect();
    // SAFETY: a NUL-terminated path; a valid handle (share mode 0) is closed, an
    // invalid one means the file is locked by Edge.
    unsafe {
        let handle = CreateFileW(
            wide.as_ptr(),
            GENERIC_READ,
            0,
            std::ptr::null(),
            OPEN_EXISTING,
            0,
            std::ptr::null_mut(),
        );
        if handle == INVALID_HANDLE_VALUE {
            return true;
        }
        CloseHandle(handle);
        false
    }
}

fn edit_preferences(preferences: &Path, edit: &impl Fn(&mut serde_json::Value)) {
    let Ok(text) = std::fs::read_to_string(preferences) else {
        return;
    };
    let Ok(mut root) = serde_json::from_str::<serde_json::Value>(&text) else {
        return;
    };
    if !root.is_object() {
        return;
    }
    edit(&mut root);
    let temporary = preferences.with_extension("fixedge-tmp");
    let is_written = serde_json::to_string(&root)
        .ok()
        .and_then(|serialized| std::fs::write(&temporary, serialized).ok())
        .is_some();
    if is_written {
        let _ = std::fs::rename(&temporary, preferences);
    }
}

// Local State lists every profile Edge knows, by folder name.
fn profile_directories(user_data: &Path) -> Vec<String> {
    let Ok(text) = std::fs::read_to_string(user_data.join("Local State")) else {
        return Vec::new();
    };
    let Ok(state) = serde_json::from_str::<serde_json::Value>(&text) else {
        return Vec::new();
    };
    state
        .get("profile")
        .and_then(|profile| profile.get("info_cache"))
        .and_then(serde_json::Value::as_object)
        .map(|cache| cache.keys().cloned().collect())
        .unwrap_or_default()
}
