//! Breaks away from any job the core runs in (a terminal, an IDE), which would
//! otherwise end the child with it.

use windows_sys::Win32::System::Threading::{
    CREATE_BREAKAWAY_FROM_JOB, CREATE_NO_WINDOW, CreateProcessW, DETACHED_PROCESS,
    PROCESS_CREATION_FLAGS, PROCESS_INFORMATION, STARTUPINFOW,
};

use crate::handle;
use crate::wide::to_wide;

const NO_EXTRA_FLAGS: PROCESS_CREATION_FLAGS = 0;

/// `CreateProcessW` detached with no window, breaking away from the job when the
/// job allows it. True when it started.
#[must_use]
pub fn start_detached(command_line: &str) -> bool {
    create(command_line, CREATE_BREAKAWAY_FROM_JOB) || create(command_line, NO_EXTRA_FLAGS)
}

// A job that forbids breaking away refuses the first form; the second still
// detaches from the console and the window.
fn create(command_line: &str, extra_flags: PROCESS_CREATION_FLAGS) -> bool {
    // CreateProcessW may write into the command line, so it gets its own copy.
    let mut command_line = to_wide(command_line);
    // SAFETY: both are plain data, for which all zeros is valid.
    let mut startup_info: STARTUPINFOW = unsafe { std::mem::zeroed() };
    startup_info.cb = u32::try_from(size_of::<STARTUPINFOW>()).unwrap_or(u32::MAX);
    // SAFETY: as above.
    let mut information: PROCESS_INFORMATION = unsafe { std::mem::zeroed() };
    let flags = DETACHED_PROCESS | CREATE_NO_WINDOW | extra_flags;
    // SAFETY: the command line is a mutable NUL-terminated buffer, and the
    // startup and process structures are live locals sized as required.
    let is_started = unsafe {
        CreateProcessW(
            std::ptr::null(),
            command_line.as_mut_ptr(),
            std::ptr::null(),
            std::ptr::null(),
            0,
            flags,
            std::ptr::null(),
            std::ptr::null(),
            &raw const startup_info,
            &raw mut information,
        )
    } != 0;
    if is_started {
        // Owned so they close at once; the child runs on without them.
        drop(handle::from_nullable(information.hProcess));
        drop(handle::from_nullable(information.hThread));
    }
    is_started
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn starts_a_detached_process() {
        assert!(start_detached("cmd.exe /c exit"));
    }

    #[test]
    fn reports_a_missing_program() {
        assert!(!start_detached(r"C:\no\such\program-edgeplus-test.exe"));
    }
}
