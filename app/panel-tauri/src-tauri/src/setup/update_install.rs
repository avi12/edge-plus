//! Swapping a downloaded exe in for the running one. A running exe cannot be
//! overwritten but can be renamed, so the new one takes its name and the old
//! one waits beside it under a retired name until the next start deletes it.

use std::ffi::OsString;
use std::fmt::Write as _;
use std::fs::{self, File};
use std::io;
use std::path::{Path, PathBuf};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use sha2::{Digest, Sha256};

use crate::platform::paths;

const STAGED_SUFFIX: &str = ".new";
const RETIRED_MARKER: &str = ".old-";
/// A .NET tick is 100 ns, counted from 0001-01-01; the retired name carries
/// ticks as the C# panel's did.
const TICKS_AT_UNIX_EPOCH: u128 = 621_355_968_000_000_000;
const NANOSECONDS_PER_TICK: u128 = 100;

/// Fails when the file cannot be read.
pub fn is_digest_of(path: &Path, sha256: &str) -> io::Result<bool> {
    let mut file = File::open(path)?;
    let mut hasher = Sha256::new();
    io::copy(&mut file, &mut hasher)?;
    Ok(upper_hex(&hasher.finalize()).eq_ignore_ascii_case(sha256))
}

fn upper_hex(bytes: &[u8]) -> String {
    bytes.iter().fold(String::new(), |mut hex, byte| {
        let _ = write!(hex, "{byte:02X}");
        hex
    })
}

/// The new exe is copied beside the running one first, so the swap itself is a
/// rename on one volume: a copy cut short never leaves a half exe under the
/// running exe's name. Fails if it cannot (a folder Edge+ may not write to),
/// with the running exe back under its own name.
pub fn install(downloaded: &Path, running: &Path, sha256: &str, now: SystemTime) -> io::Result<()> {
    let staged = with_suffix(running, STAGED_SUFFIX);
    fs::copy(downloaded, &staged)?;
    if !is_digest_of(&staged, sha256)? {
        let _ = fs::remove_file(&staged);
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "the staged update does not match its digest",
        ));
    }
    let retired = retired_name(running, now);
    fs::rename(running, &retired)?;
    if let Err(error) = fs::rename(&staged, running) {
        fs::rename(&retired, running)?;
        return Err(error);
    }
    Ok(())
}

#[must_use]
pub fn retired_name(running: &Path, now: SystemTime) -> PathBuf {
    with_suffix(running, &format!("{RETIRED_MARKER}{}", ticks(now)))
}

/// After a restart, the exe that was swapped out is no longer running.
pub fn delete_leftovers(running: &Path) {
    let (Some(folder), Some(name)) = (running.parent(), running.file_name()) else {
        return;
    };
    let name = name.to_string_lossy();
    let retired_prefix = paths::fold_case(&format!("{name}{RETIRED_MARKER}"));
    let staged = paths::fold_case(&format!("{name}{STAGED_SUFFIX}"));
    let Ok(entries) = fs::read_dir(folder) else {
        return;
    };
    for entry in entries.flatten() {
        let is_file = entry.file_type().is_ok_and(|file_type| file_type.is_file());
        let file_name = paths::fold_case(&entry.file_name().to_string_lossy());
        let is_leftover = file_name.starts_with(&retired_prefix) || file_name == staged;
        if is_file && is_leftover {
            let _ = fs::remove_file(entry.path());
        }
    }
}

fn with_suffix(path: &Path, suffix: &str) -> PathBuf {
    let mut name = OsString::from(path.as_os_str());
    name.push(suffix);
    PathBuf::from(name)
}

fn ticks(now: SystemTime) -> u128 {
    let since_epoch = now.duration_since(UNIX_EPOCH).unwrap_or(Duration::ZERO);
    TICKS_AT_UNIX_EPOCH + since_epoch.as_nanos() / NANOSECONDS_PER_TICK
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Plain files under a throwaway temp folder stand in for the exes.
    struct TempFolder(PathBuf);

    impl TempFolder {
        fn new(name: &str) -> Self {
            let folder =
                std::env::temp_dir().join(format!("edgeplus-update-{name}-{}", std::process::id()));
            let _ = fs::remove_dir_all(&folder);
            fs::create_dir_all(&folder).expect("create the temp folder");
            Self(folder)
        }
    }

    impl Drop for TempFolder {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }

    fn digest(contents: &str) -> String {
        upper_hex(&Sha256::digest(contents.as_bytes()))
    }

    fn write(folder: &Path, name: &str, contents: &str) -> PathBuf {
        fs::create_dir_all(folder).expect("create the folder");
        let path = folder.join(name);
        fs::write(&path, contents).expect("write the file");
        path
    }

    fn read(path: &Path) -> String {
        fs::read_to_string(path).expect("read the file")
    }

    #[test]
    fn checks_a_digest() {
        let temp = TempFolder::new("digest");
        let file = write(&temp.0, "digest.exe", "new build");
        assert!(
            is_digest_of(&file, &digest("new build")).expect("reads"),
            "the contents' digest"
        );
        assert!(
            is_digest_of(&file, &digest("new build").to_ascii_lowercase()).expect("reads"),
            "in either case"
        );
        assert!(
            !is_digest_of(&file, &digest("another build")).expect("reads"),
            "not another's"
        );
    }

    #[test]
    fn swaps_the_exe_and_keeps_the_old_one_aside() {
        let temp = TempFolder::new("swap");
        let running = write(&temp.0.join("swap"), "edgeplus.exe", "old build");
        let downloaded = write(
            &temp.0.join("swap-update"),
            "EdgePlus-0.0.2.exe",
            "new build",
        );
        // 2026-10-24 12:00 UTC.
        let now = UNIX_EPOCH + Duration::from_hours(498_012);

        install(&downloaded, &running, &digest("new build"), now).expect("installs");

        assert_eq!(
            "new build",
            read(&running),
            "the new build has the exe's name"
        );
        assert_eq!(
            "old build",
            read(&retired_name(&running, now)),
            "the old one waits beside it under a retired name"
        );
        assert!(
            !with_suffix(&running, STAGED_SUFFIX).exists(),
            "nothing is left staged"
        );
        assert!(
            downloaded.exists(),
            "the download is left for its owner to clear"
        );
    }

    #[test]
    fn refuses_a_wrong_digest_and_changes_nothing() {
        let temp = TempFolder::new("wrong");
        let folder = temp.0.join("wrong");
        let running = write(&folder, "edgeplus.exe", "old build");
        let downloaded = write(
            &temp.0.join("wrong-update"),
            "EdgePlus-0.0.2.exe",
            "tampered",
        );

        let result = install(
            &downloaded,
            &running,
            &digest("new build"),
            SystemTime::now(),
        );

        assert_eq!(
            Some(io::ErrorKind::InvalidData),
            result.err().map(|error| error.kind()),
            "a download that is not what the manifest named is refused"
        );
        assert_eq!("old build", read(&running), "the running exe is as it was");
        assert_eq!(
            1,
            fs::read_dir(&folder).expect("list").count(),
            "and nothing else is left beside it"
        );
    }

    #[test]
    fn leaves_the_running_exe_alone_when_staging_fails() {
        let temp = TempFolder::new("fail");
        let running = write(&temp.0.join("fail"), "edgeplus.exe", "old build");
        let downloaded = write(
            &temp.0.join("fail-update"),
            "EdgePlus-0.0.2.exe",
            "new build",
        );
        // A directory where the staged copy goes makes the copy itself fail.
        fs::create_dir_all(with_suffix(&running, STAGED_SUFFIX)).expect("block the staging");

        let result = install(
            &downloaded,
            &running,
            &digest("new build"),
            SystemTime::now(),
        );

        assert!(result.is_err(), "a swap that cannot stage its copy fails");
        assert_eq!(
            "old build",
            read(&running),
            "with the running exe untouched"
        );
    }

    #[test]
    fn deletes_what_the_swap_left() {
        let temp = TempFolder::new("clean");
        let running = write(&temp.0, "edgeplus.exe", "current");
        let retired = write(&temp.0, "edgeplus.exe.old-123", "old");
        let staged = write(&temp.0, "edgeplus.exe.new", "staged");
        let other = write(&temp.0, "settings.json", "keep");

        delete_leftovers(&running);

        assert!(
            !retired.exists() && !staged.exists(),
            "retired and staged copies go"
        );
        assert!(
            running.exists() && other.exists(),
            "the exe and other files stay"
        );
        delete_leftovers(&running);
        assert!(running.exists(), "running it again is harmless");
    }
}
