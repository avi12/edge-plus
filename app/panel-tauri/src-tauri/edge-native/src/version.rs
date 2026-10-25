//! A binary's file version from its version resource's fixed info
//! (VS_FIXEDFILEINFO), as the DLL's self-check reads it (verify.h).

use std::path::Path;

/// [major, minor, build, revision]; None for a missing file or one with no
/// version resource.
#[must_use]
pub fn file_version(path: &Path) -> Option<[u16; 4]> {
    let _ = path;
    todo!()
}
