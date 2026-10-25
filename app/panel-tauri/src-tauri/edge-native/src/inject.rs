//! LoadLibraryW on a remote thread. Unloading is the DLL's own job, on its
//! unload event (sync).

use std::path::Path;

/// Injects `dll` into each process; returns how many took it. A re-inject only
/// bumps the module's reference count, so it is harmless.
pub fn inject_into(pids: &[u32], dll: &Path) -> usize {
    let _ = (pids, dll);
    todo!()
}

/// A DLL that unloaded itself but was re-injected meanwhile keeps an extra
/// reference: hooks gone, unload event closed, yet still mapped. FreeLibrary is
/// called (up to 16 times) until the module is gone.
pub fn free_module(pid: u32, module: &str) {
    let _ = (pid, module);
    todo!()
}
