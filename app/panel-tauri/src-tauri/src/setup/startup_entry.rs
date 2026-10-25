//! The Run key is the one source of truth, never remembered elsewhere.

use std::io;
use std::path::Path;

/// Moves an entry an earlier panel (Tauri or WinUI) wrote over to
/// `executable`; true when it moved one.
pub fn migrate_to(executable: &Path) -> io::Result<bool> {
    let _ = executable;
    todo!()
}
