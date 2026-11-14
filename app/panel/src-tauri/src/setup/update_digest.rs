//! Checking a downloaded update against the digest the manifest names.

use std::fmt::Write as _;
use std::fs::File;
use std::io;
use std::path::Path;

use sha2::{Digest, Sha256};

/// Fails when the file cannot be read.
pub(crate) fn is_digest_of(path: &Path, sha256: &str) -> io::Result<bool> {
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

#[cfg(test)]
mod tests {
    use super::*;

    fn digest(contents: &str) -> String {
        upper_hex(&Sha256::digest(contents.as_bytes()))
    }

    #[test]
    fn checks_a_digest() {
        let file = std::env::temp_dir().join(format!("edgeplus-digest-{}", std::process::id()));
        std::fs::write(&file, "new build").expect("write the file");
        let checks = [
            (digest("new build"), true, "the contents' digest"),
            (
                digest("new build").to_ascii_lowercase(),
                true,
                "in either case",
            ),
            (digest("another build"), false, "not another's"),
        ];
        for (expected, is_match, why) in checks {
            assert_eq!(
                is_match,
                is_digest_of(&file, &expected).expect("reads"),
                "{why}"
            );
        }
        let _ = std::fs::remove_file(&file);
    }
}
