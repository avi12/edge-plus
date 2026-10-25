//! A binary's file version, read from its version resource's fixed info as the
//! DLL's self-check does (verify.h), so the panel and the DLL agree on what
//! "the same build" means.

use std::fmt;
use std::path::Path;

const PART_COUNT: usize = 4;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct FileVersion {
    pub major: u16,
    pub minor: u16,
    pub build: u16,
    pub revision: u16,
}

impl FileVersion {
    #[must_use]
    pub const fn new(major: u16, minor: u16, build: u16, revision: u16) -> Self {
        Self {
            major,
            minor,
            build,
            revision,
        }
    }

    /// Exactly four dot-separated numbers that each fit a 16-bit part.
    #[must_use]
    pub fn parse(text: &str) -> Option<Self> {
        let parts: Vec<&str> = text.trim().split('.').collect();
        if parts.len() != PART_COUNT {
            return None;
        }
        let mut numbers = [0_u16; PART_COUNT];
        for (number, part) in numbers.iter_mut().zip(&parts) {
            let is_digits = !part.is_empty() && part.bytes().all(|byte| byte.is_ascii_digit());
            if !is_digits {
                return None;
            }
            *number = part.parse().ok()?;
        }
        let [major, minor, build, revision] = numbers;
        Some(Self::new(major, minor, build, revision))
    }

    /// None for a missing file or one with no version resource.
    #[must_use]
    pub fn of(path: &Path) -> Option<Self> {
        let [major, minor, build, revision] = edge_native::file_version(path)?;
        let version = Self::new(major, minor, build, revision);
        // A binary with no version resource reads as all zeros.
        let has_version_resource = version != Self::default();
        has_version_resource.then_some(version)
    }
}

impl Default for FileVersion {
    fn default() -> Self {
        Self::new(0, 0, 0, 0)
    }
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

#[cfg(test)]
mod tests {
    use super::*;

    fn version(text: &str) -> FileVersion {
        FileVersion::parse(text).expect("parses")
    }

    #[test]
    fn parses_four_part_versions_only() {
        assert_eq!(FileVersion::new(155, 0, 4283, 45), version("155.0.4283.45"));
        assert_eq!("155.0.4283.45", version(" 155.0.4283.45 ").to_string());
        for text in [
            "",
            "155",
            "155.0.4283",
            "155.0.4283.45.1",
            "155.0.x.45",
            "155.0.4283.70000",
            "155.0.+4283.45",
        ] {
            assert!(FileVersion::parse(text).is_none(), "\"{text}\" does not parse");
        }
    }

    #[test]
    fn orders_by_part() {
        assert!(version("155.0.4283.45") < version("155.0.4284.2"));
        assert!(version("156.0.0.0") > version("155.9.9999.9"));
    }

    #[test]
    fn reads_a_system_binary() {
        let system = std::env::var_os("SystemRoot").expect("SystemRoot");
        let kernel32 = Path::new(&system).join("System32").join("kernel32.dll");
        assert!(FileVersion::of(&kernel32).is_some_and(|read| read.build > 0));
        assert!(FileVersion::of(&std::env::temp_dir().join("no-such.dll")).is_none());
    }
}
