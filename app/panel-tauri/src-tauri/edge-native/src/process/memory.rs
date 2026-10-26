//! The windowless listener only watches Edge, so the pages startup touched go
//! back to Windows.

use windows_sys::Win32::System::Threading::{GetCurrentProcess, SetProcessWorkingSetSize};

// (-1, -1) tells SetProcessWorkingSetSize to empty the working set.
const TRIM_ALL: usize = usize::MAX;

/// SetProcessWorkingSetSize(-1, -1).
pub fn trim_working_set() {
    // SAFETY: the current-process pseudo handle needs no closing.
    unsafe {
        SetProcessWorkingSetSize(GetCurrentProcess(), TRIM_ALL, TRIM_ALL);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn trims_without_panicking() {
        trim_working_set();
    }
}
