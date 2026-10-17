//! The native-split DLL carried inside the panel binary.
//!
//! The DLL is embedded at build time (build.rs builds it and copies it into
//! `OUT_DIR`) and unpacked next to the panel's data, so the switch always
//! injects the DLL that matches this build. A copy currently loaded in Edge
//! holds the file open; unpacking then keeps the loaded copy rather than
//! failing - the next inject picks up the fresh one after the next eject.

use crate::paths;

const DLL_BYTES: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/fixedge-split.dll"));

/// Whether the unpacked DLL is byte-identical to the one embedded in this build.
pub fn is_unpacked_current() -> bool {
    std::fs::read(paths::split_dll()).is_ok_and(|existing| existing == DLL_BYTES)
}

/// Write the embedded DLL to its data-folder path unless an identical copy is
/// already there. A write failure (the DLL is loaded and the file is locked) is
/// ignored - the loaded copy stays.
pub fn unpack() {
    let target = paths::split_dll();
    if is_unpacked_current() {
        return;
    }
    if let Some(parent) = target.parent() {
        let _ = std::fs::create_dir_all(parent);
    }
    let _ = std::fs::write(&target, DLL_BYTES);
}
