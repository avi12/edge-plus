//! A binary's file version (`major.minor.build.revision`), read from its
//! version resource the same way the DLL's own self-check does
//! (`app/native-split/src/verify.h`), so the panel and the DLL agree on what
//! "the same build" means.

use std::fmt;
use std::os::windows::ffi::OsStrExt;
use std::path::Path;
use std::str::FromStr;

use windows_sys::Win32::Storage::FileSystem::{
    GetFileVersionInfoSizeW, GetFileVersionInfoW, VerQueryValueW, VS_FIXEDFILEINFO,
};

// VerQueryValueW's sub-block for the VS_FIXEDFILEINFO root.
const FIXED_FILE_INFO_BLOCK: &str = "\\";
const PART_COUNT: usize = 4;
const WORD_BITS: u32 = 16;

/// A four-part file version, compared part by part.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct FileVersion {
    pub major: u16,
    pub minor: u16,
    pub build: u16,
    pub revision: u16,
}

impl fmt::Display for FileVersion {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            formatter,
            "{}.{}.{}.{}",
            self.major, self.minor, self.build, self.revision
        )
    }
}

/// Why a version string could not be read.
#[derive(Debug, PartialEq, Eq)]
pub struct ParseVersionError(String);

impl fmt::Display for ParseVersionError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "not a four-part version: {:?}", self.0)
    }
}

impl std::error::Error for ParseVersionError {}

impl FromStr for FileVersion {
    type Err = ParseVersionError;

    fn from_str(text: &str) -> Result<Self, Self::Err> {
        let error = || ParseVersionError(text.to_owned());
        let parts: Vec<u16> = text
            .trim()
            .split('.')
            .map(str::parse)
            .collect::<Result<_, _>>()
            .map_err(|_| error())?;
        let [major, minor, build, revision] =
            <[u16; PART_COUNT]>::try_from(parts).map_err(|_| error())?;
        Ok(Self {
            major,
            minor,
            build,
            revision,
        })
    }
}

impl FileVersion {
    // The four words of VS_FIXEDFILEINFO's dwFileVersionMS / dwFileVersionLS.
    fn from_words(most_significant: u32, least_significant: u32) -> Self {
        let high = |word: u32| u16::try_from(word >> WORD_BITS).unwrap_or(0);
        let low = |word: u32| u16::try_from(word & 0xffff).unwrap_or(0);
        Self {
            major: high(most_significant),
            minor: low(most_significant),
            build: high(least_significant),
            revision: low(least_significant),
        }
    }
}

/// The file version in the version resource of the binary at `path`, or `None`
/// when it has none or cannot be read.
#[must_use]
pub fn file_version(path: &Path) -> Option<FileVersion> {
    let wide_path: Vec<u16> = path.as_os_str().encode_wide().chain([0]).collect();
    let block: Vec<u16> = FIXED_FILE_INFO_BLOCK.encode_utf16().chain([0]).collect();
    // SAFETY: both strings are NUL-terminated and outlive the calls; the
    // resource buffer is sized by GetFileVersionInfoSizeW and owned here, and
    // VerQueryValueW's out pointer points into it, read only while it lives.
    unsafe {
        let mut ignored = 0u32;
        let size = GetFileVersionInfoSizeW(wide_path.as_ptr(), std::ptr::addr_of_mut!(ignored));
        if size == 0 {
            return None;
        }
        let mut buffer = vec![0u8; size as usize];
        let has_info =
            GetFileVersionInfoW(wide_path.as_ptr(), 0, size, buffer.as_mut_ptr().cast()) != 0;
        if !has_info {
            return None;
        }
        let mut info: *mut std::ffi::c_void = std::ptr::null_mut();
        let mut info_length = 0u32;
        let has_fixed_info = VerQueryValueW(
            buffer.as_ptr().cast(),
            block.as_ptr(),
            std::ptr::addr_of_mut!(info),
            std::ptr::addr_of_mut!(info_length),
        ) != 0
            && !info.is_null();
        if !has_fixed_info {
            return None;
        }
        let fixed = &*info.cast::<VS_FIXEDFILEINFO>();
        Some(FileVersion::from_words(
            fixed.dwFileVersionMS,
            fixed.dwFileVersionLS,
        ))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_a_four_part_version() {
        let parsed: FileVersion = "155.0.4283.45".parse().expect("valid version");
        assert_eq!(
            parsed,
            FileVersion {
                major: 155,
                minor: 0,
                build: 4283,
                revision: 45
            }
        );
        assert_eq!(parsed.to_string(), "155.0.4283.45");
    }

    #[test]
    fn rejects_anything_but_four_numeric_parts() {
        for text in [
            "",
            "155",
            "155.0.4283",
            "155.0.4283.45.1",
            "155.0.x.45",
            "155.0.4283.70000",
        ] {
            assert!(
                text.parse::<FileVersion>().is_err(),
                "{text:?} should not parse"
            );
        }
    }

    #[test]
    fn orders_by_part() {
        let older: FileVersion = "155.0.4283.45".parse().expect("valid version");
        let newer: FileVersion = "155.0.4284.2".parse().expect("valid version");
        assert!(older < newer);
    }

    #[test]
    fn splits_the_fixed_info_words() {
        let version = FileVersion::from_words(0x009b_0000, 0x10bb_002d);
        assert_eq!(version.to_string(), "155.0.4283.45");
    }

    #[test]
    fn reads_a_system_binary() {
        // Windows reports kernel32's version through a compatibility shim (6.2
        // to an unmanifested caller), so only the build number is asserted.
        let windows = std::env::var_os("SystemRoot").expect("SystemRoot");
        let kernel = Path::new(&windows).join("System32").join("kernel32.dll");
        let version = file_version(&kernel).expect("kernel32 has a version resource");
        assert!(version.build > 0, "{version}");
    }
}
