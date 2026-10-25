//! A binary's file version from its version resource's fixed info
//! (`VS_FIXEDFILEINFO`), as the DLL's self-check reads it (verify.h).

use std::ffi::c_void;
use std::path::Path;

use windows_sys::Win32::Storage::FileSystem::{
    GetFileVersionInfoSizeW, GetFileVersionInfoW, VS_FIXEDFILEINFO, VerQueryValueW,
};

use crate::wide::to_wide;

// VerQueryValueW's sub-block for the VS_FIXEDFILEINFO root.
const FIXED_FILE_INFO_BLOCK: &str = "\\";
const WORD_BITS: u32 = 16;

/// [major, minor, build, revision]; None for a missing file or one with no
/// version resource.
#[must_use]
pub fn file_version(path: &Path) -> Option<[u16; 4]> {
    let wide_path = to_wide(path);
    // SAFETY: the path is NUL-terminated and lives across the call; the
    // handle out-parameter is optional.
    let size = unsafe { GetFileVersionInfoSizeW(wide_path.as_ptr(), std::ptr::null_mut()) };
    if size == 0 {
        return None;
    }
    let mut resource = vec![0u8; size as usize];
    // SAFETY: the buffer is exactly the size GetFileVersionInfoSizeW asked for.
    let has_resource =
        unsafe { GetFileVersionInfoW(wide_path.as_ptr(), 0, size, resource.as_mut_ptr().cast()) }
            != 0;
    if !has_resource {
        return None;
    }
    let block = to_wide(FIXED_FILE_INFO_BLOCK);
    let mut info: *mut c_void = std::ptr::null_mut();
    let mut info_length = 0u32;
    // SAFETY: the resource buffer was filled above; info points into it.
    let is_found = unsafe {
        VerQueryValueW(
            resource.as_ptr().cast(),
            block.as_ptr(),
            &raw mut info,
            &raw mut info_length,
        )
    } != 0;
    let is_whole_fixed_info =
        is_found && !info.is_null() && info_length as usize >= size_of::<VS_FIXEDFILEINFO>();
    if !is_whole_fixed_info {
        return None;
    }
    // SAFETY: info points at a whole VS_FIXEDFILEINFO inside resource, which
    // lives until the end of this function; read_unaligned needs no alignment.
    let fixed = unsafe { info.cast::<VS_FIXEDFILEINFO>().read_unaligned() };
    Some([
        high_word(fixed.dwFileVersionMS),
        low_word(fixed.dwFileVersionMS),
        high_word(fixed.dwFileVersionLS),
        low_word(fixed.dwFileVersionLS),
    ])
}

fn high_word(double_word: u32) -> u16 {
    u16::try_from(double_word >> WORD_BITS).unwrap_or(u16::MAX)
}

fn low_word(double_word: u32) -> u16 {
    u16::try_from(double_word & u32::from(u16::MAX)).unwrap_or(u16::MAX)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn splits_the_fixed_info_words() {
        assert_eq!([high_word(0x009b_0000), low_word(0x009b_0000)], [155, 0]);
        assert_eq!([high_word(0x10bb_002d), low_word(0x10bb_002d)], [4283, 45]);
    }

    #[test]
    fn reads_a_system_binary() {
        // Windows may report kernel32's major and minor through a compatibility
        // shim to an unmanifested caller, so only the build number is asserted.
        let windows = std::env::var_os("SystemRoot").expect("SystemRoot is set");
        let kernel32 = Path::new(&windows).join("System32").join("kernel32.dll");
        let [_, _, build, _] = file_version(&kernel32).expect("kernel32 has a version");
        assert!(build > 0);
    }

    #[test]
    fn reads_nothing_from_a_missing_file() {
        assert!(file_version(Path::new(r"C:\no\such\edgeplus-test.dll")).is_none());
    }
}
