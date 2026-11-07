//! Named events and mutexes.

use std::os::windows::io::{AsRawHandle, OwnedHandle};

use windows_sys::Win32::Foundation::{ERROR_ALREADY_EXISTS, GetLastError};
use windows_sys::Win32::System::Threading::{
    CreateMutexW, EVENT_MODIFY_STATE, OpenEventW, ReleaseMutex, SYNCHRONIZATION_SYNCHRONIZE,
    SetEvent,
};

use crate::kernel::handle;
use crate::kernel::wide::to_wide;

/// Whether the named event exists (`OpenEventW` succeeds).
#[must_use]
pub fn is_event_open(name: &str) -> bool {
    open_event(name).is_some()
}

/// Sets the named event; a no-op when it does not exist.
pub fn signal_event(name: &str) {
    let Some(event) = open_event(name) else {
        return;
    };
    // SAFETY: the event is open with EVENT_MODIFY_STATE until dropped below.
    unsafe {
        SetEvent(event.as_raw_handle());
    }
}

// The access .NET's EventWaitHandle.TryOpenExisting asks for.
fn open_event(name: &str) -> Option<OwnedHandle> {
    let name = to_wide(name);
    // SAFETY: the name is NUL-terminated and lives across the call; a null
    // result becomes None.
    handle::from_nullable(unsafe {
        OpenEventW(
            EVENT_MODIFY_STATE | SYNCHRONIZATION_SYNCHRONIZE,
            0,
            name.as_ptr(),
        )
    })
}

/// Held until dropped.
pub struct NamedMutex {
    mutex: OwnedHandle,
}

impl NamedMutex {
    /// None when another process already holds one of this name.
    #[must_use]
    pub fn acquire_sole(name: &str) -> Option<Self> {
        let name = to_wide(name);
        // SAFETY: the name is NUL-terminated and lives across the call; a null
        // result becomes None. GetLastError is read straight after, on the
        // same thread.
        let (mutex, last_error) = unsafe {
            let mutex = CreateMutexW(std::ptr::null(), 1, name.as_ptr());
            (handle::from_nullable(mutex), GetLastError())
        };
        // Opening an existing mutex does not take it, whatever the initial
        // owner asked for; the handle closes as it drops here.
        let is_held_elsewhere = last_error == ERROR_ALREADY_EXISTS;
        if is_held_elsewhere {
            return None;
        }
        mutex.map(|mutex| Self { mutex })
    }
}

impl Drop for NamedMutex {
    fn drop(&mut self) {
        // SAFETY: this handle created the mutex owned; the handle itself closes
        // when the field drops right after.
        unsafe {
            ReleaseMutex(self.mutex.as_raw_handle());
        }
    }
}

#[cfg(test)]
mod tests {
    use windows_sys::Win32::System::Threading::{CreateEventW, WaitForSingleObject};

    use super::*;

    const SIGNALED: u32 = 0;

    fn test_name(purpose: &str) -> String {
        format!(
            r"Local\edgeplus-native-test-{purpose}-{}",
            std::process::id()
        )
    }

    fn create_event(name: &str) -> OwnedHandle {
        let name = to_wide(name);
        // SAFETY: the name is NUL-terminated and lives across the call.
        let event = unsafe { CreateEventW(std::ptr::null(), 1, 0, name.as_ptr()) };
        handle::from_nullable(event).expect("the test event is created")
    }

    #[test]
    fn opens_and_signals_an_event() {
        let name = test_name("event");
        assert!(!is_event_open(&name));
        let event = create_event(&name);
        assert!(is_event_open(&name));
        signal_event(&name);
        // SAFETY: the event is open until the end of the test.
        let wait = unsafe { WaitForSingleObject(event.as_raw_handle(), 0) };
        assert_eq!(wait, SIGNALED);
        drop(event);
        assert!(!is_event_open(&name));
    }

    #[test]
    fn signals_a_missing_event_as_a_no_op() {
        signal_event(&test_name("missing"));
    }

    #[test]
    fn holds_a_mutex_alone_until_dropped() {
        let name = test_name("mutex");
        let first = NamedMutex::acquire_sole(&name).expect("the first holder gets it");
        assert!(NamedMutex::acquire_sole(&name).is_none());
        drop(first);
        assert!(NamedMutex::acquire_sole(&name).is_some());
    }
}
