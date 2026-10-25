//! The split DLL embedded in the exe, unpacked to the data folder so the
//! injected DLL always matches this build. A copy loaded in Edge holds its file
//! open: unpacking then keeps it, and the next inject after an eject picks up
//! the fresh one.

use std::fs;

use crate::platform::paths;

/// build.rs rebuilds the DLL when its sources are newer and copies it here.
const DLL: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/edgeplus-split.dll"));

#[must_use]
pub fn is_unpacked_current() -> bool {
    fs::read(paths::split_dll()).is_ok_and(|unpacked| unpacked == DLL)
}

pub fn unpack() {
    if is_unpacked_current() {
        return;
    }
    let _ = fs::create_dir_all(paths::data_root());
    let _ = fs::write(paths::split_dll(), DLL);
}
