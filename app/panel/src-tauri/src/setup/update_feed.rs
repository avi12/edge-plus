//! Reads the release feed the app's GitHub releases carry (`releases.json`,
//! written by winui-svelte's `createRelease`): the versions released, each with
//! its zip, SHA-256 and size. The feed is the latest release's asset, so the
//! app polls one small file and nothing is offered unless it names a newer
//! version. A release without a sound digest is never offered, since a download
//! that cannot be checked is never installed.

use std::net::IpAddr;

use serde::Deserialize;
use ureq::http::Uri;
use winui_svelte_launcher::Version;

pub const DEFAULT_URL: &str =
    "https://github.com/avi12/edge-plus/releases/latest/download/releases.json";
/// Set by a test to point the check at a local stand-in for GitHub.
pub const URL_VARIABLE: &str = "EDGEPLUS_UPDATE_FEED";

const SHA256_HEX_LENGTH: usize = 64;
const HTTPS_SCHEME: &str = "https";
const HTTP_SCHEME: &str = "http";
const LOCALHOST: &str = "localhost";
const PATH_SEPARATOR: char = '/';

/// A release newer than the running one: its zip, where it is and how to check
/// it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct UpdateOffer {
    pub version: Version,
    pub file: String,
    pub download: String,
    /// Upper-case hex.
    pub sha256: String,
    pub size: u64,
}

#[derive(Deserialize)]
struct Feed {
    releases: Vec<Release>,
}

#[derive(Deserialize)]
struct Release {
    version: String,
    file: String,
    sha256: String,
    size: u64,
}

#[must_use]
pub fn url() -> String {
    std::env::var(URL_VARIABLE)
        .ok()
        .filter(|custom| !custom.is_empty())
        .unwrap_or_else(|| DEFAULT_URL.to_owned())
}

/// The newest release of the feed at `feed_url` when it is newer than `current`.
#[must_use]
pub fn parse(json: &str, feed_url: &str, current: Version) -> Option<UpdateOffer> {
    let feed: Feed = serde_json::from_str(json).ok()?;
    let newest = feed
        .releases
        .iter()
        .filter_map(|release| Some((Version::parse(&release.version).ok()?, release)))
        .max_by_key(|(version, _)| *version)?;
    let (version, release) = newest;
    if version <= current {
        return None;
    }
    offer(version, release, feed_url)
}

fn offer(version: Version, release: &Release, feed_url: &str) -> Option<UpdateOffer> {
    let digest = &release.sha256;
    let has_digest =
        digest.len() == SHA256_HEX_LENGTH && digest.bytes().all(|byte| byte.is_ascii_hexdigit());
    // A name with a separator would unpack outside the packages folder.
    let is_plain_name =
        !release.file.is_empty() && !release.file.contains([PATH_SEPARATOR, '\\', ':']);
    if !has_digest || !is_plain_name {
        return None;
    }
    let (folder, _) = feed_url.rsplit_once(PATH_SEPARATOR)?;
    let download = format!("{folder}{PATH_SEPARATOR}{}", release.file);
    let uri: Uri = download.parse().ok()?;
    if !is_trusted(&uri) {
        return None;
    }
    Some(UpdateOffer {
        version,
        file: release.file.clone(),
        download,
        sha256: digest.to_ascii_uppercase(),
        size: release.size,
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

#[cfg(test)]
mod tests {
    use super::*;

    const FEED_URL: &str = "https://example.com/latest/download/releases.json";

    fn running() -> Version {
        Version::new(0, 0, 1)
    }

    fn digest(letter: char) -> String {
        letter.to_string().repeat(SHA256_HEX_LENGTH)
    }

    fn feed(version: &str, file: &str, digest: &str) -> String {
        format!(
            r#"{{"releases":[{{"version":"{version}","file":"{file}","sha256":"{digest}","size":7}}]}}"#
        )
    }

    fn offers(json: &str, feed_url: &str) -> bool {
        parse(json, feed_url, running()).is_some()
    }

    #[test]
    fn offers_the_newest_release_beside_the_feed() {
        let json = format!(
            r#"{{"releases":[{{"version":"0.0.2","file":"a.zip","sha256":"{0}","size":1}},{{"version":"0.0.3","file":"b.zip","sha256":"{0}","size":2}}]}}"#,
            digest('a')
        );
        let offer = parse(&json, FEED_URL, running()).expect("a newer version is offered");
        assert_eq!(Version::new(0, 0, 3), offer.version, "the newest");
        assert_eq!(
            "https://example.com/latest/download/b.zip", offer.download,
            "its zip is fetched from beside the feed"
        );
        assert_eq!(digest('A'), offer.sha256, "with its digest, upper case");
        assert_eq!(2, offer.size, "and its size");
        assert!(
            offers(
                &feed("0.0.2", "e.zip", &digest('a')),
                "http://localhost:8080/releases.json"
            ),
            "a stand-in server on this machine is trusted over http"
        );
    }

    #[test]
    fn offers_nothing_otherwise() {
        let checks = [
            (
                feed("0.0.1", "e.zip", &digest('a')),
                FEED_URL,
                "the running version is not an update",
            ),
            (
                feed("0.0.0", "e.zip", &digest('a')),
                FEED_URL,
                "an older one is not either",
            ),
            (
                feed("0.0.2", "e.zip", ""),
                FEED_URL,
                "without a digest nothing is offered",
            ),
            (
                feed("0.0.2", "e.zip", &"a".repeat(63)),
                FEED_URL,
                "a digest of the wrong length is refused",
            ),
            (
                feed("0.0.2", "e.zip", &digest('z')),
                FEED_URL,
                "and one that is not hex",
            ),
            (
                feed("0.0.2", "../e.zip", &digest('a')),
                FEED_URL,
                "a file name that climbs out is refused",
            ),
            (
                feed("0.0.2", "e.zip", &digest('a')),
                "http://example.com/releases.json",
                "a plain http feed from elsewhere is refused",
            ),
            (
                feed("soon", "e.zip", &digest('a')),
                FEED_URL,
                "a version that is not one is refused",
            ),
            ("{}".to_owned(), FEED_URL, "an empty feed offers nothing"),
            (
                "[1,2]".to_owned(),
                FEED_URL,
                "a feed of the wrong shape neither",
            ),
            ("not json".to_owned(), FEED_URL, "nor one that is not json"),
        ];
        for (json, feed_url, why) in checks {
            assert!(!offers(&json, feed_url), "{why}");
        }
    }
}
