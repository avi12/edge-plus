//! `LoadLibraryW` on a remote thread. Unloading is the DLL's own job, on its
//! unload event (sync).

use std::ffi::c_void;
use std::os::windows::io::{AsRawHandle, OwnedHandle};
use std::path::Path;

use windows_sys::Win32::System::Diagnostics::Debug::WriteProcessMemory;
use windows_sys::Win32::System::LibraryLoader::{GetModuleHandleW, GetProcAddress};
use windows_sys::Win32::System::Memory::{
    MEM_COMMIT, MEM_RELEASE, MEM_RESERVE, PAGE_READWRITE, VirtualAllocEx, VirtualFreeEx,
};
use windows_sys::Win32::System::Threading::{
    CreateRemoteThread, LPTHREAD_START_ROUTINE, OpenProcess, PROCESS_ACCESS_RIGHTS,
    PROCESS_CREATE_THREAD, PROCESS_QUERY_INFORMATION, PROCESS_VM_OPERATION, PROCESS_VM_READ,
    PROCESS_VM_WRITE, WaitForSingleObject,
};

use crate::kernel::handle;
use crate::kernel::wide::to_wide;
use crate::process::browsers::module_base;

const REMOTE_THREAD_TIMEOUT_MILLISECONDS: u32 = 10_000;
// A DLL that unloaded itself but was re-injected meanwhile can hold several
// references; this many FreeLibrary calls clears any real count.
const MAX_REFERENCES: usize = 16;
const KERNEL32: &str = "kernel32.dll";
const THREAD_ACCESS: PROCESS_ACCESS_RIGHTS =
    PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION;
const MEMORY_ACCESS: PROCESS_ACCESS_RIGHTS = PROCESS_VM_WRITE | PROCESS_VM_READ;

/// The kernel32 exports a remote thread is started on.
#[derive(Clone, Copy)]
enum RemoteFunction {
    LoadLibrary,
    FreeLibrary,
}

impl RemoteFunction {
    fn export_name(self) -> &'static std::ffi::CStr {
        match self {
            Self::LoadLibrary => c"LoadLibraryW",
            Self::FreeLibrary => c"FreeLibrary",
        }
    }
}

/// Injects `dll` into each process; returns how many took it. A re-inject only
/// bumps the module's reference count, so it is harmless.
#[must_use]
pub fn inject_into(pids: &[u32], dll: &Path) -> usize {
    let wide_path = to_wide(dll);
    pids.iter()
        .filter(|&&pid| inject_one(pid, &wide_path))
        .count()
}

/// A DLL that unloaded itself but was re-injected meanwhile keeps an extra
/// reference: hooks gone, unload event closed, yet still mapped, so
/// `LoadLibrary` would only bump the count and never run `DllMain` again.
/// `FreeLibrary` is called (up to 16 times) until the module is gone.
pub fn free_module(pid: u32, module: &str) {
    for _ in 0..MAX_REFERENCES {
        let Some(base) = module_base(pid, module) else {
            return;
        };
        let Some(process) = open_process(pid, THREAD_ACCESS) else {
            return;
        };
        if !run_remote_thread(&process, RemoteFunction::FreeLibrary, base as *const c_void) {
            return;
        }
    }
}

fn inject_one(pid: u32, wide_path: &[u16]) -> bool {
    let Some(process) = open_process(pid, THREAD_ACCESS | MEMORY_ACCESS) else {
        return false;
    };
    let Some(remote_path) = RemoteAllocation::new(&process, size_of_val(wide_path)) else {
        return false;
    };
    // SAFETY: the process is open with VM_WRITE; the remote allocation is
    // exactly the path's size, and the source slice lives across the call.
    let is_path_written = unsafe {
        WriteProcessMemory(
            process.as_raw_handle(),
            remote_path.address,
            wide_path.as_ptr().cast(),
            size_of_val(wide_path),
            std::ptr::null_mut(),
        )
    } != 0;
    if !is_path_written {
        return false;
    }
    run_remote_thread(&process, RemoteFunction::LoadLibrary, remote_path.address)
}

fn open_process(pid: u32, access: PROCESS_ACCESS_RIGHTS) -> Option<OwnedHandle> {
    // SAFETY: OpenProcess takes plain values; a null result becomes None.
    handle::from_nullable(unsafe { OpenProcess(access, 0, pid) })
}

// kernel32 is mapped at the same address in every process of a boot, so this
// process's export address is the remote one too.
fn run_remote_thread(
    process: &OwnedHandle,
    function: RemoteFunction,
    argument: *const c_void,
) -> bool {
    let kernel32 = to_wide(KERNEL32);
    // SAFETY: kernel32 is loaded in every process, and both names are
    // NUL-terminated and live across the calls.
    let export = unsafe {
        GetProcAddress(
            GetModuleHandleW(kernel32.as_ptr()),
            function.export_name().as_ptr().cast(),
        )
    };
    let Some(export) = export else {
        return false;
    };
    // SAFETY: LoadLibraryW and FreeLibrary both take one pointer-sized
    // argument and return a value no wider than a register, so each is a valid
    // thread start routine; only the signature's type changes.
    let start = unsafe {
        std::mem::transmute::<unsafe extern "system" fn() -> isize, LPTHREAD_START_ROUTINE>(export)
    };
    // SAFETY: the process is open with CREATE_THREAD; start is the remote
    // address of the export and argument is valid in that process.
    let thread = unsafe {
        CreateRemoteThread(
            process.as_raw_handle(),
            std::ptr::null(),
            0,
            start,
            argument,
            0,
            std::ptr::null_mut(),
        )
    };
    let Some(thread) = handle::from_nullable(thread) else {
        return false;
    };
    // SAFETY: the thread handle is open until dropped below.
    unsafe {
        WaitForSingleObject(thread.as_raw_handle(), REMOTE_THREAD_TIMEOUT_MILLISECONDS);
    }
    true
}

/// Memory committed in another process, released on drop.
struct RemoteAllocation<'process> {
    process: &'process OwnedHandle,
    address: *mut c_void,
}

impl<'process> RemoteAllocation<'process> {
    fn new(process: &'process OwnedHandle, size: usize) -> Option<Self> {
        // SAFETY: the process is open with VM_OPERATION; a null result is
        // handled below.
        let address = unsafe {
            VirtualAllocEx(
                process.as_raw_handle(),
                std::ptr::null(),
                size,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_READWRITE,
            )
        };
        if address.is_null() {
            return None;
        }
        Some(Self { process, address })
    }
}

impl Drop for RemoteAllocation<'_> {
    fn drop(&mut self) {
        // SAFETY: the allocation was made in that process by new and is
        // released once, while the borrowed process handle is still open.
        unsafe {
            VirtualFreeEx(self.process.as_raw_handle(), self.address, 0, MEM_RELEASE);
        }
    }
}
