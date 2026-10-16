//! Loading and unloading the native-split DLL in Edge's browser processes.
//!
//! Turning a feature on injects `fixedge-split.dll` with the classic
//! `LoadLibraryW`-on-a-remote-thread move; turning everything off signals the
//! DLL's unload event, and it removes its hooks and drops out of Edge with no
//! restart. The event existing is exactly "the DLL is loaded right now", so the
//! panel never tracks per-process state. Mirrors the old `NativeSplit.cs`.

use std::io;
use std::os::windows::ffi::OsStrExt;
use std::path::Path;

use windows_sys::Win32::Foundation::{CloseHandle, HANDLE};
use windows_sys::Win32::System::LibraryLoader::{GetModuleHandleW, GetProcAddress};
use windows_sys::Win32::System::Memory::{
    VirtualAllocEx, VirtualFreeEx, MEM_COMMIT, MEM_RELEASE, MEM_RESERVE, PAGE_READWRITE,
};
use windows_sys::Win32::System::Threading::{
    CreateRemoteThread, OpenEventW, OpenProcess, SetEvent, WaitForSingleObject, EVENT_MODIFY_STATE,
    PROCESS_CREATE_THREAD, PROCESS_QUERY_INFORMATION, PROCESS_VM_OPERATION, PROCESS_VM_READ,
    PROCESS_VM_WRITE,
};

// Must match kUnloadEvent in app/native-split/src/dllmain.cpp.
const UNLOAD_EVENT: &str = r"Local\fixedge-split-unload";
const LOAD_LIBRARY_TIMEOUT_MS: u32 = 10_000;

/// Load `dll_path` into each of `pids` that does not already hold it. Returns
/// how many injections were issued. Injecting into a process that already has
/// the module only bumps its reference count, so a re-inject is harmless.
///
/// # Errors
/// Returns the first OS error that stops an injection from being set up.
pub fn inject_into(pids: &[u32], dll_path: &Path) -> io::Result<usize> {
    let wide_path: Vec<u16> = dll_path.as_os_str().encode_wide().chain([0]).collect();
    let mut injected = 0usize;
    for &pid in pids {
        if inject_one(pid, &wide_path)? {
            injected += 1;
        }
    }
    Ok(injected)
}

/// Whether the native-split DLL is loaded in any Edge process right now (its
/// unload event is open for the DLL's whole lifetime).
#[must_use]
pub fn is_split_loaded() -> bool {
    let handle = open_unload_event();
    if handle.is_null() {
        return false;
    }
    // SAFETY: a valid event handle from OpenEventW is closed once.
    unsafe { CloseHandle(handle) };
    true
}

/// Signal the DLL to remove its hooks and unload itself (a no-op if none is
/// loaded).
pub fn signal_unload() {
    let handle = open_unload_event();
    if handle.is_null() {
        return;
    }
    // SAFETY: a valid, modify-state event handle is set then closed.
    unsafe {
        SetEvent(handle);
        CloseHandle(handle);
    }
}

fn open_unload_event() -> HANDLE {
    let name: Vec<u16> = UNLOAD_EVENT.encode_utf16().chain([0]).collect();
    // SAFETY: a null return is handled by the caller; the name is NUL-terminated.
    unsafe { OpenEventW(EVENT_MODIFY_STATE, 0, name.as_ptr()) }
}

fn inject_one(pid: u32, wide_path: &[u16]) -> io::Result<bool> {
    let byte_len = std::mem::size_of_val(wide_path);
    // SAFETY: every handle/allocation opened below is released on every path out
    // of this block; pointers passed to the Win32 calls point at live locals.
    unsafe {
        let process = OpenProcess(
            PROCESS_CREATE_THREAD
                | PROCESS_QUERY_INFORMATION
                | PROCESS_VM_OPERATION
                | PROCESS_VM_WRITE
                | PROCESS_VM_READ,
            0,
            pid,
        );
        if process.is_null() {
            return Ok(false);
        }
        let result = inject_into_open_process(process, wide_path, byte_len);
        CloseHandle(process);
        result
    }
}

unsafe fn inject_into_open_process(
    process: HANDLE,
    wide_path: &[u16],
    byte_len: usize,
) -> io::Result<bool> {
    let remote = VirtualAllocEx(
        process,
        std::ptr::null(),
        byte_len,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE,
    );
    if remote.is_null() {
        return Err(io::Error::last_os_error());
    }
    let written = write_remote_memory(process, remote, wide_path.as_ptr().cast(), byte_len);
    if !written {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return Err(io::Error::last_os_error());
    }
    let kernel32: Vec<u16> = "kernel32.dll".encode_utf16().chain([0]).collect();
    let module = GetModuleHandleW(kernel32.as_ptr());
    let load_library = GetProcAddress(module, c"LoadLibraryW".as_ptr().cast());
    let Some(load_library) = load_library else {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return Err(io::Error::last_os_error());
    };
    let thread = CreateRemoteThread(
        process,
        std::ptr::null(),
        0,
        Some(std::mem::transmute::<
            unsafe extern "system" fn() -> isize,
            unsafe extern "system" fn(*mut std::ffi::c_void) -> u32,
        >(load_library)),
        remote,
        0,
        std::ptr::null_mut(),
    );
    if !thread.is_null() {
        WaitForSingleObject(thread, LOAD_LIBRARY_TIMEOUT_MS);
        CloseHandle(thread);
    }
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    Ok(!thread.is_null())
}

// windows-sys's WriteProcessMemory wants an optional out-count; wrap it so the
// call site reads cleanly and always passes null for the count we ignore.
unsafe fn write_remote_memory(
    process: HANDLE,
    address: *mut std::ffi::c_void,
    buffer: *const std::ffi::c_void,
    size: usize,
) -> bool {
    windows_sys::Win32::System::Diagnostics::Debug::WriteProcessMemory(
        process,
        address,
        buffer,
        size,
        std::ptr::null_mut(),
    ) != 0
}
