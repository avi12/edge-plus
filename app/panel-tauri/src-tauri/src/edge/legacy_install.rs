//! What an install from before the rename leaves behind (the project was called
//! fix-edge): its split DLL, still loaded in Edge, and its data folder.

pub fn run() {
    todo!()
}

/// A process the old DLL is still hooked in is left alone.
#[must_use]
pub fn is_dll_loaded_in(pid: u32) -> bool {
    let _ = pid;
    todo!()
}
