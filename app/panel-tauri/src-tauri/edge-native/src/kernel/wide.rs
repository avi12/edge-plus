//! UTF-16 strings, as every W-suffixed Win32 call takes and returns them.

use std::ffi::{OsStr, OsString};
use std::os::windows::ffi::{OsStrExt, OsStringExt};

/// NUL-terminated, for a `PCWSTR` parameter.
pub(crate) fn to_wide(text: impl AsRef<OsStr>) -> Vec<u16> {
    text.as_ref().encode_wide().chain([0]).collect()
}

/// A fixed-size buffer the API filled, up to its first NUL.
pub(crate) fn from_wide(buffer: &[u16]) -> OsString {
    let end = buffer
        .iter()
        .position(|&unit| unit == 0)
        .unwrap_or(buffer.len());
    OsString::from_wide(&buffer[..end])
}
