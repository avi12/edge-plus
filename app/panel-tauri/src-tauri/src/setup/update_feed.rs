//! Reads the update manifest the repository hosts (docs/updates.json, written
//! by tools/Write-UpdateManifest.ps1 once a release's files are up): the latest
//! version, where its exe and its MSI are, and each one's SHA-256. It is the
//! same arrangement as a browser extension's update URL: the app polls a small
//! file and nothing is offered unless the file names a newer version. A file
//! without a digest is never offered, since a download that cannot be checked
//! is never installed in place of the running exe.

use std::cmp::Ordering;
use std::fmt;
use std::net::IpAddr;

use serde_json::Value;
use ureq::http::Uri;

pub const DEFAULT_URL: &str =
    "https://raw.githubusercontent.com/avi12/edge-plus/main/docs/updates.json";
/// Set by a test to point the check at a local stand-in for GitHub.
pub const URL_VARIABLE: &str = "EDGEPLUS_UPDATE_FEED";

const SHA256_HEX_LENGTH: usize = 64;
const VERSION_PROPERTY: &str = "version";
const URL_PROPERTY: &str = "url";
const SHA256_PROPERTY: &str = "sha256";
const INSTALLER_PROPERTY: &str = "installer";
const HTTPS_SCHEME: &str = "https";
const HTTP_SCHEME: &str = "http";
const LOCALHOST: &str = "localhost";

/// One file of a release: where to fetch it and the digest to check it by.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct UpdateAsset {
    pub download: String,
    /// Upper-case hex.
    pub sha256: String,
}

/// A release newer than the running one: its portable exe, and its MSI for a
/// copy the MSI installed (None when the manifest names none).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct UpdateOffer {
    pub version: Version,
    pub exe: UpdateAsset,
    pub installer: Option<UpdateAsset>,
}

/// Compares and parses as .NET's System.Version, which the earlier panel used:
/// two to four parts, and a missing part orders before a zero (0.0.1 is older
/// than 0.0.1.0).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Version {
    parts: [i32; MAX_VERSION_PARTS],
}

const MIN_VERSION_PARTS: usize = 2;
const MAX_VERSION_PARTS: usize = 4;
const MISSING_PART: i32 = -1;
/// What .NET's integer parsing skips around a number.
const NUMBER_WHITESPACE: [char; 6] = [' ', '\t', '\n', '\u{b}', '\u{c}', '\r'];

impl Version {
    #[must_use]
    pub fn parse(text: &str) -> Option<Self> {
        let pieces: Vec<&str> = text.split('.').collect();
        let is_part_count_valid = (MIN_VERSION_PARTS..=MAX_VERSION_PARTS).contains(&pieces.len());
        if !is_part_count_valid {
            return None;
        }
        let mut parts = [MISSING_PART; MAX_VERSION_PARTS];
        for (part, piece) in parts.iter_mut().zip(&pieces) {
            let number: i32 = piece.trim_matches(NUMBER_WHITESPACE).parse().ok()?;
            if number < 0 {
                return None;
            }
            *part = number;
        }
        Some(Self { parts })
    }
}

impl Ord for Version {
    fn cmp(&self, other: &Self) -> Ordering {
        self.parts.cmp(&other.parts)
    }
}

impl PartialOrd for Version {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl fmt::Display for Version {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        let present: Vec<String> = self
            .parts
            .iter()
            .take_while(|&&part| part != MISSING_PART)
            .map(ToString::to_string)
            .collect();
        formatter.write_str(&present.join("."))
    }
}

#[must_use]
pub fn url() -> String {
    std::env::var(URL_VARIABLE)
        .ok()
        .filter(|custom| !custom.is_empty())
        .unwrap_or_else(|| DEFAULT_URL.to_owned())
}

#[must_use]
pub fn parse(json: &str, current: Version) -> Option<UpdateOffer> {
    let manifest: Value = serde_json::from_str(json).ok()?;
    offer(&manifest, current)
}

fn offer(manifest: &Value, current: Version) -> Option<UpdateOffer> {
    let version = Version::parse(text(manifest, VERSION_PROPERTY))?;
    let exe = asset(manifest)?;
    if version <= current {
        return None;
    }
    let installer = manifest.get(INSTALLER_PROPERTY).and_then(asset);
    Some(UpdateOffer {
        version,
        exe,
        installer,
    })
}

/// The `url` and `sha256` of a file, or None unless both are sound.
fn asset(element: &Value) -> Option<UpdateAsset> {
    let digest = text(element, SHA256_PROPERTY);
    let has_digest =
        digest.len() == SHA256_HEX_LENGTH && digest.bytes().all(|byte| byte.is_ascii_hexdigit());
    if !has_digest {
        return None;
    }
    let url: Uri = text(element, URL_PROPERTY).parse().ok()?;
    if !is_trusted(&url) {
        return None;
    }
    Some(UpdateAsset {
        download: url.to_string(),
        sha256: digest.to_ascii_uppercase(),
    })
}

/// Over https, or from this machine for a test's stand-in server.
fn is_trusted(url: &Uri) -> bool {
    match url.scheme_str() {
        Some(HTTPS_SCHEME) => url.host().is_some(),
        Some(HTTP_SCHEME) => url.host().is_some_and(is_loopback),
        _ => false,
    }
}

fn is_loopback(host: &str) -> bool {
    if host.eq_ignore_ascii_case(LOCALHOST) {
        return true;
    }
    let address = host.trim_start_matches('[').trim_end_matches(']');
    address
        .parse::<IpAddr>()
        .is_ok_and(|address| address.is_loopback())
}

/// A string property, else empty (missing, another type, or not an object).
fn text<'value>(element: &'value Value, name: &str) -> &'value str {
    element.get(name).and_then(Value::as_str).unwrap_or("")
}

#[cfg(test)]
mod tests {
    use super::*;

    const EXE_URL: &str = "https://example.com/e.exe";
    const MSI_URL: &str = "https://example.com/e-setup.msi";

    fn running() -> Version {
        Version::parse("0.0.1").expect("parses")
    }

    fn digest(letter: char) -> String {
        letter.to_string().repeat(SHA256_HEX_LENGTH)
    }

    fn manifest(version: &str, url: &str, digest: &str) -> String {
        format!(r#"{{"version":"{version}","url":"{url}","sha256":"{digest}"}}"#)
    }

    fn offers(json: &str) -> bool {
        parse(json, running()).is_some()
    }

    #[test]
    fn offers_a_newer_release() {
        let release =
            "https://github.com/avi12/edge-plus/releases/download/v0.0.2/EdgePlus-0.0.2.exe";
        let offer = parse(&manifest("0.0.2", release, &digest('a')), running())
            .expect("a newer version is offered");
        assert_eq!(
            Version::parse("0.0.2"),
            Some(offer.version),
            "with its version"
        );
        assert_eq!(release, offer.exe.download, "and where to fetch it");
        assert_eq!(digest('A'), offer.exe.sha256, "and its digest, upper case");
        assert!(
            offer.installer.is_none(),
            "a manifest naming no installer offers none"
        );

        assert!(
            offers(&manifest("0.1", EXE_URL, &digest('a'))),
            "a two-part version is a version too"
        );
        assert!(
            offers(&manifest(
                "0.0.2",
                "http://localhost:8080/e.exe",
                &digest('a')
            )),
            "a stand-in server on this machine is trusted over http"
        );
        assert!(
            offers(&manifest(
                "0.0.2",
                "http://127.0.0.1:8080/e.exe",
                &digest('a')
            )),
            "so is the loopback address"
        );
    }

    #[test]
    fn offers_nothing_otherwise() {
        let checks = [
            (
                manifest("0.0.1", EXE_URL, &digest('a')),
                "the version already running is not an update",
            ),
            (
                manifest("0.0.0", EXE_URL, &digest('a')),
                "an older one is not either",
            ),
            (
                manifest("0.0.2", EXE_URL, ""),
                "without a digest nothing is offered",
            ),
            (
                manifest("0.0.2", EXE_URL, &"a".repeat(63)),
                "a digest of the wrong length is refused",
            ),
            (
                manifest("0.0.2", EXE_URL, &digest('z')),
                "and one that is not hex",
            ),
            (
                manifest("0.0.2", "http://example.com/e.exe", &digest('a')),
                "a plain http download from elsewhere is refused",
            ),
            (
                manifest("0.0.2", "not a url", &digest('a')),
                "a url that is not one is refused",
            ),
            (
                manifest("soon", EXE_URL, &digest('a')),
                "a version that is not one is refused",
            ),
            ("{}".to_owned(), "an empty manifest offers nothing"),
            ("[1,2]".to_owned(), "a manifest of the wrong shape neither"),
            ("not json".to_owned(), "nor one that is not json"),
            (String::new(), "nor an empty answer"),
        ];
        for (json, why) in checks {
            assert!(!offers(&json), "{why}");
        }
    }

    #[test]
    fn offers_the_installer_beside_the_exe() {
        let exe_digest = digest('a');
        let with_installer = format!(
            r#"{{"version":"0.0.2","url":"{EXE_URL}","sha256":"{exe_digest}","installer":{{"url":"{MSI_URL}","sha256":"{}"}}}}"#,
            digest('b')
        );
        let offer = parse(&with_installer, running()).expect("offered");
        let installer = offer
            .installer
            .expect("the installer is offered beside the exe");
        assert_eq!(MSI_URL, installer.download, "with where to fetch it");
        assert_eq!(digest('B'), installer.sha256, "and its own digest");

        let bad_installer = format!(
            r#"{{"version":"0.0.2","url":"{EXE_URL}","sha256":"{exe_digest}","installer":{{"url":"{MSI_URL}"}}}}"#
        );
        let without_digest = parse(&bad_installer, running());
        assert!(
            without_digest.is_some_and(|offer| offer.installer.is_none()),
            "an installer without a digest is dropped, the exe still offered"
        );

        let only_installer = format!(
            r#"{{"version":"0.0.2","installer":{{"url":"{MSI_URL}","sha256":"{exe_digest}"}}}}"#
        );
        assert!(
            !offers(&only_installer),
            "a release with no exe is not offered"
        );
    }

    #[test]
    fn versions_compare_as_dotnet_does() {
        let version = |text: &str| Version::parse(text).expect("parses");
        assert!(version("0.1") > version("0.0.1"));
        assert!(
            version("0.0.1") < version("0.0.1.0"),
            "a missing part is before a zero"
        );
        assert!(version("1.10.0") > version("1.9.0"));
        assert_eq!("0.0.2", version("0.0.2").to_string());
        assert_eq!("1.2", version(" 1 . 2 ").to_string());
        for text in ["", "1", "1.2.3.4.5", "1.-2", "1.x", "1..2"] {
            assert!(
                Version::parse(text).is_none(),
                "\"{text}\" is not a version"
            );
        }
    }
}
