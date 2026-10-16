//! Finding Edge's browser processes.
//!
//! Edge runs one browser process per profile plus many renderer/GPU/utility
//! children, all named `msedge.exe`. Only the browser process owns a top-level
//! window, so - exactly as the old C# panel did with `MainWindowHandle != 0` -
//! we take the `msedge.exe` pids that own a visible, titled top-level window.

use std::collections::HashSet;
use std::os::windows::ffi::OsStringExt;
use std::path::Path;

use windows_sys::Win32::Foundation::{
    CloseHandle, BOOL, HWND, INVALID_HANDLE_VALUE, LPARAM, MAX_PATH, TRUE,
};
use windows_sys::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Module32FirstW, Module32NextW, MODULEENTRY32W, TH32CS_SNAPMODULE,
    TH32CS_SNAPMODULE32,
};
use windows_sys::Win32::System::Threading::{
    OpenProcess, QueryFullProcessImageNameW, PROCESS_NAME_WIN32, PROCESS_QUERY_LIMITED_INFORMATION,
};
use windows_sys::Win32::UI::WindowsAndMessaging::{
    EnumWindows, GetWindowTextLengthW, GetWindowThreadProcessId, IsWindowVisible,
};

const EDGE_IMAGE_NAME: &str = "msedge.exe";

/// The pids of Edge's browser processes (one per open profile), or empty when
/// Edge is not running.
#[must_use]
pub fn edge_browser_pids() -> Vec<u32> {
    let mut candidates: Vec<u32> = Vec::new();
    // SAFETY: `EnumWindows` calls `collect_window` for each top-level window with
    // our Vec as the lparam; the closure only pushes pids and never escapes.
    unsafe {
        EnumWindows(
            Some(collect_window),
            std::ptr::addr_of_mut!(candidates) as LPARAM,
        );
    }
    candidates
        .into_iter()
        .filter(|&pid| is_edge_process(pid))
        .collect::<HashSet<u32>>()
        .into_iter()
        .collect()
}

unsafe extern "system" fn collect_window(window: HWND, lparam: LPARAM) -> BOOL {
    let is_shown = IsWindowVisible(window) != 0 && GetWindowTextLengthW(window) != 0;
    if is_shown {
        let mut process_id: u32 = 0;
        GetWindowThreadProcessId(window, std::ptr::addr_of_mut!(process_id));
        if process_id != 0 {
            let pids = &mut *(lparam as *mut Vec<u32>);
            pids.push(process_id);
        }
    }
    TRUE
}

/// The pids of Edge browser processes that do NOT already have a module named
/// `module_basename` loaded - the ones an injection still needs to reach. This
/// fixes the old panel's gap where a second profile opened after the DLL loaded
/// was never injected (the "is it loaded" check was global, not per-process).
#[must_use]
pub fn edge_pids_missing_module(module_basename: &str) -> Vec<u32> {
    edge_browser_pids()
        .into_iter()
        .filter(|&pid| !has_module(pid, module_basename))
        .collect()
}

fn has_module(pid: u32, module_basename: &str) -> bool {
    // SAFETY: the snapshot handle is created, walked and closed in this scope;
    // each MODULEENTRY32W is zeroed with its dwSize set as the API requires.
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if snapshot == INVALID_HANDLE_VALUE {
            return false;
        }
        let mut entry: MODULEENTRY32W = std::mem::zeroed();
        entry.dwSize = u32::try_from(std::mem::size_of::<MODULEENTRY32W>()).unwrap_or(0);
        let mut found = false;
        let mut more = Module32FirstW(snapshot, std::ptr::addr_of_mut!(entry));
        while more != 0 {
            if module_name_matches(&entry.szModule, module_basename) {
                found = true;
                break;
            }
            more = Module32NextW(snapshot, std::ptr::addr_of_mut!(entry));
        }
        CloseHandle(snapshot);
        found
    }
}

fn module_name_matches(wide: &[u16], basename: &str) -> bool {
    let end = wide.iter().position(|&c| c == 0).unwrap_or(wide.len());
    let name = String::from_utf16_lossy(&wide[..end]);
    name.eq_ignore_ascii_case(basename)
}

fn is_edge_process(pid: u32) -> bool {
    image_basename(pid).is_some_and(|name| name.eq_ignore_ascii_case(EDGE_IMAGE_NAME))
}

fn image_basename(pid: u32) -> Option<String> {
    // SAFETY: a limited-info handle is opened, queried and closed in this scope;
    // the buffer length is passed by pointer and clamped by the API.
    unsafe {
        let process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
        if process.is_null() {
            return None;
        }
        let mut buffer = [0u16; MAX_PATH as usize];
        let mut length = u32::try_from(buffer.len()).unwrap_or(0);
        let ok = QueryFullProcessImageNameW(
            process,
            PROCESS_NAME_WIN32,
            buffer.as_mut_ptr(),
            std::ptr::addr_of_mut!(length),
        );
        CloseHandle(process);
        if ok == 0 {
            return None;
        }
        let full = std::ffi::OsString::from_wide(&buffer[..length as usize]);
        Path::new(&full)
            .file_name()
            .map(|name| name.to_string_lossy().into_owned())
    }
}
