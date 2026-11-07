//! Kernel handles closed on drop, through std's `OwnedHandle`.

use std::os::windows::io::{FromRawHandle, OwnedHandle};

use windows_sys::Win32::Foundation::{HANDLE, INVALID_HANDLE_VALUE};

/// Takes ownership of a handle from an API that returns null on failure
/// (`OpenProcess`, `OpenEventW`, `CreateMutexW`, `CreateRemoteThread`, ...).
pub(crate) fn from_nullable(handle: HANDLE) -> Option<OwnedHandle> {
    if handle.is_null() {
        return None;
    }
    // SAFETY: a non-null handle just returned by the API is open and owned by
    // nobody else, so the OwnedHandle is its sole closer.
    Some(unsafe { OwnedHandle::from_raw_handle(handle) })
}

/// Takes ownership of a handle from an API that returns
/// `INVALID_HANDLE_VALUE` on failure (`CreateToolhelp32Snapshot`).
pub(crate) fn from_invalidable(handle: HANDLE) -> Option<OwnedHandle> {
    if handle == INVALID_HANDLE_VALUE {
        return None;
    }
    from_nullable(handle)
}
