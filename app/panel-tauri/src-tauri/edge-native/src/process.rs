//! Of all the msedge.exe processes (one browser per profile plus renderer, GPU
//! and utility children), only a browser process owns a visible, titled
//! top-level window: that is how the browser processes are told apart.

use std::collections::HashSet;
use std::os::windows::io::AsRawHandle;
use std::path::PathBuf;
use std::time::Duration;

use windows_sys::Win32::Foundation::{BOOL, HWND, LPARAM, MAX_PATH, TRUE};
use windows_sys::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, MODULEENTRY32W, Module32FirstW, Module32NextW, TH32CS_SNAPMODULE,
    TH32CS_SNAPMODULE32,
};
use windows_sys::Win32::System::Threading::{
    OpenProcess, PROCESS_NAME_WIN32, PROCESS_QUERY_LIMITED_INFORMATION, PROCESS_SYNCHRONIZE,
    QueryFullProcessImageNameW, WaitForSingleObject,
};
use windows_sys::Win32::UI::WindowsAndMessaging::{
    EnumWindows, GetWindowTextLengthW, GetWindowThreadProcessId, IsWindowVisible,
};

use crate::handle;
use crate::wide::from_wide;

const EDGE_IMAGE_NAME: &str = "msedge.exe";
// u32::MAX is INFINITE to WaitForSingleObject; a longer timeout stops just short.
const LONGEST_WAIT_MILLISECONDS: u32 = u32::MAX - 1;

/// The pids of Edge's browser processes, or empty when Edge is not running.
#[must_use]
pub fn browser_pids() -> Vec<u32> {
    let mut candidates: HashSet<u32> = HashSet::new();
    // SAFETY: EnumWindows calls collect_window synchronously, before it
    // returns, with the HashSet's address as the lparam; nothing keeps it.
    unsafe {
        EnumWindows(
            Some(collect_window),
            std::ptr::addr_of_mut!(candidates) as LPARAM,
        );
    }
    candidates
        .into_iter()
        .filter(|&pid| is_edge_process(pid))
        .collect()
}

unsafe extern "system" fn collect_window(window: HWND, candidates: LPARAM) -> BOOL {
    // SAFETY: window is the live top-level window EnumWindows passes in.
    let is_shown = unsafe { IsWindowVisible(window) != 0 && GetWindowTextLengthW(window) != 0 };
    if is_shown {
        let mut process_id: u32 = 0;
        // SAFETY: as above; the out pointer is a live local.
        unsafe {
            GetWindowThreadProcessId(window, std::ptr::addr_of_mut!(process_id));
        }
        if process_id != 0 {
            // SAFETY: browser_pids passes its HashSet as the lparam and holds
            // it, unaliased, for the whole EnumWindows call.
            let candidates = unsafe { &mut *(candidates as *mut HashSet<u32>) };
            candidates.insert(process_id);
        }
    }
    TRUE
}

/// The browser processes with `module` loaded.
#[must_use]
pub fn pids_with_module(module: &str) -> Vec<u32> {
    browser_pids()
        .into_iter()
        .filter(|&pid| find_module(pid, module).is_some())
        .collect()
}

/// Per process, so a second profile opened after the DLL loaded is still
/// injected.
#[must_use]
pub fn pids_missing_module(module: &str) -> Vec<u32> {
    browser_pids()
        .into_iter()
        .filter(|&pid| find_module(pid, module).is_none())
        .collect()
}

/// Edge keeps each version in its own folder, so this names the build the
/// process is actually running.
#[must_use]
pub fn module_path(pid: u32, module: &str) -> Option<PathBuf> {
    find_module(pid, module).map(|entry| PathBuf::from(from_wide(&entry.szExePath)))
}

#[must_use]
pub fn module_base(pid: u32, module: &str) -> Option<usize> {
    find_module(pid, module).map(|entry| entry.modBaseAddr as usize)
}

/// Returns at once when the process is already gone.
pub fn wait_for_exit(pid: u32, timeout: Duration) {
    // SAFETY: OpenProcess takes plain values; a null result is handled below.
    let process = handle::from_nullable(unsafe { OpenProcess(PROCESS_SYNCHRONIZE, 0, pid) });
    let Some(process) = process else {
        return;
    };
    let milliseconds = u32::try_from(timeout.as_millis())
        .unwrap_or(u32::MAX)
        .min(LONGEST_WAIT_MILLISECONDS);
    // SAFETY: the process handle is open, with SYNCHRONIZE, until dropped below.
    unsafe {
        WaitForSingleObject(process.as_raw_handle(), milliseconds);
    }
}

fn find_module(pid: u32, module: &str) -> Option<MODULEENTRY32W> {
    // SAFETY: CreateToolhelp32Snapshot takes plain values; an invalid handle
    // is handled below.
    let snapshot =
        unsafe { CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid) };
    let snapshot = handle::from_invalidable(snapshot)?;
    // SAFETY: MODULEENTRY32W is plain data, for which all zeros is valid.
    let mut entry: MODULEENTRY32W = unsafe { std::mem::zeroed() };
    entry.dwSize = u32::try_from(size_of::<MODULEENTRY32W>()).unwrap_or(u32::MAX);
    // SAFETY: the snapshot is open until dropped, and entry is a live local
    // with dwSize set, as Module32FirstW/NextW require.
    let mut has_more = unsafe { Module32FirstW(snapshot.as_raw_handle(), &raw mut entry) } != 0;
    while has_more {
        let is_named_module = from_wide(&entry.szModule).eq_ignore_ascii_case(module);
        if is_named_module {
            return Some(entry);
        }
        // SAFETY: as above.
        has_more = unsafe { Module32NextW(snapshot.as_raw_handle(), &raw mut entry) } != 0;
    }
    None
}

fn is_edge_process(pid: u32) -> bool {
    image_path(pid).is_some_and(|path| {
        path.file_name()
            .is_some_and(|name| name.eq_ignore_ascii_case(EDGE_IMAGE_NAME))
    })
}

fn image_path(pid: u32) -> Option<PathBuf> {
    // SAFETY: OpenProcess takes plain values; a null result is handled below.
    let process =
        handle::from_nullable(unsafe { OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid) })?;
    let mut buffer = [0u16; MAX_PATH as usize];
    let mut length = u32::try_from(buffer.len()).unwrap_or(u32::MAX);
    // SAFETY: the process handle is open until dropped; the buffer and its
    // length in characters describe the same live array.
    let is_named = unsafe {
        QueryFullProcessImageNameW(
            process.as_raw_handle(),
            PROCESS_NAME_WIN32,
            buffer.as_mut_ptr(),
            &raw mut length,
        )
    } != 0;
    if !is_named {
        return None;
    }
    Some(PathBuf::from(from_wide(&buffer)))
}

#[cfg(test)]
mod tests {
    use std::process::Command;
    use std::time::Instant;

    use super::*;

    #[test]
    fn lists_browser_pids_without_panicking() {
        let pids = browser_pids();
        assert!(pids.iter().all(|&pid| pid != 0));
    }

    #[test]
    fn finds_kernel32_in_this_process() {
        let pid = std::process::id();
        assert!(module_base(pid, "KERNEL32.dll").is_some());
        let path = module_path(pid, "kernel32.dll").expect("kernel32 is loaded");
        assert!(
            path.ends_with("kernel32.dll") || path.ends_with("KERNEL32.DLL"),
            "{path:?}"
        );
        assert!(module_base(pid, "no-such-module.dll").is_none());
    }

    #[test]
    fn waits_for_a_finished_child() {
        let mut child = Command::new("cmd")
            .args(["/c", "exit"])
            .spawn()
            .expect("cmd starts");
        let pid = child.id();
        child.wait().expect("cmd ends");
        let started = Instant::now();
        wait_for_exit(pid, Duration::from_secs(30));
        assert!(started.elapsed() < Duration::from_secs(5));
    }

    #[test]
    fn returns_at_once_for_a_missing_pid() {
        let started = Instant::now();
        // Pids are multiples of four, so this one never exists.
        wait_for_exit(u32::MAX - 2, Duration::from_secs(30));
        assert!(started.elapsed() < Duration::from_secs(5));
    }
}
