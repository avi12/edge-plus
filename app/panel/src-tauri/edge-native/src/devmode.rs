//! Snoozing Edge's "turn off extensions in developer mode" prompt.
//!
//! Edge keeps the snooze in each profile's `Preferences` as
//! `extensions.ui.dev_mode_warning_snooze_end_time` (microseconds since 1601).
//! Edge rewrites `Preferences` from memory while it runs, so the edit only
//! sticks once Edge has fully exited - which is exactly when the prompt is next
//! checked. The panel re-runs itself headless with a switch to do the waiting,
//! so it survives the panel window closing. Mirrors the old `DevModePrompt.cs`.

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
const WAITER_MUTEX: &str = r"Local\fix-edge.dev-mode-prompt";
const SNOOZE_KEY_PATH: [&str; 3] = ["extensions", "ui", "dev_mode_warning_snooze_end_time"];
const POLL: Duration = Duration::from_secs(2);
// A hundred years, in microseconds - "never ask again" in practice.
const SNOOZE_MICROSECONDS: u64 = 100 * 365 * 24 * 60 * 60 * 1_000_000;
const GENERIC_READ: u32 = 0x8000_0000;

/// Edge's default user-data directory, where its profiles live.
#[must_use]
pub fn default_user_data() -> PathBuf {
    let local = std::env::var_os("LOCALAPPDATA").map_or_else(|| PathBuf::from("."), PathBuf::from);
    local.join("Microsoft").join("Edge").join("User Data")
}

/// Wait until the Edge at `user_data` has fully exited, then write the snooze
/// into every one of its profiles. No-op if another waiter already holds the
/// single-waiter mutex.
pub fn wait_for_edge_exit_then_snooze(user_data: &Path) {
    if !claim_sole_waiter() {
        return;
    }
    while is_in_use(user_data) {
        sleep(POLL);
    }
    snooze_every_profile(user_data);
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

fn snooze_every_profile(user_data: &Path) {
    let snooze = (now_microseconds_since_1601() + SNOOZE_MICROSECONDS).to_string();
    for profile in profile_directories(user_data) {
        let preferences = user_data.join(&profile).join("Preferences");
        write_snooze(&preferences, &snooze);
    }
}

fn write_snooze(preferences: &Path, snooze: &str) {
    let Ok(text) = std::fs::read_to_string(preferences) else {
        return;
    };
    let Ok(mut root) = serde_json::from_str::<serde_json::Value>(&text) else {
        return;
    };
    if !root.is_object() {
        return;
    }
    let mut node = &mut root;
    for key in &SNOOZE_KEY_PATH[..SNOOZE_KEY_PATH.len() - 1] {
        node = node
            .as_object_mut()
            .unwrap_or_else(|| unreachable!("checked object above / inserted below"))
            .entry((*key).to_owned())
            .or_insert_with(|| serde_json::Value::Object(serde_json::Map::new()));
        if !node.is_object() {
            *node = serde_json::Value::Object(serde_json::Map::new());
        }
    }
    if let Some(object) = node.as_object_mut() {
        object.insert(
            SNOOZE_KEY_PATH[SNOOZE_KEY_PATH.len() - 1].to_owned(),
            serde_json::Value::String(snooze.to_owned()),
        );
    }
    let temporary = preferences.with_extension("fixedge-tmp");
    if serde_json::to_string(&root)
        .ok()
        .and_then(|serialized| std::fs::write(&temporary, serialized).ok())
        .is_some()
    {
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

fn now_microseconds_since_1601() -> u64 {
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
