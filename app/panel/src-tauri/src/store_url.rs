//! Pulling a theme's identity out of a pasted store URL.
//!
//! Only the real stores are recognised, each by its exact host and the path
//! before the theme (`SITES`): `<prefix>/<slug>/<id>` or `<prefix>/<id>`, where
//! `<id>` is 32 characters of `a..p`. Pure string logic, no network; the
//! `WinUI` panel's `StoreUrl.cs` mirrors it.

use edge_native::Store;

/// What the panel says when a pasted link is not a theme link from either
/// store.
pub const NOT_A_STORE_LINK: &str =
    "That is not a theme link from the Chrome Web Store or Edge Add-ons.";

const WWW_PREFIX: &str = "www.";

/// A store's host and the path segments in front of a theme's slug and id.
struct StoreSite {
    host: &'static str,
    prefix: &'static [&'static str],
    store: Store,
}

/// The one table of the sites a theme link may come from.
const SITES: [StoreSite; 3] = [
    StoreSite {
        host: "chromewebstore.google.com",
        prefix: &["detail"],
        store: Store::Chrome,
    },
    // The Chrome Web Store's old address, still in links people keep.
    StoreSite {
        host: "chrome.google.com",
        prefix: &["webstore", "detail"],
        store: Store::Chrome,
    },
    StoreSite {
        host: "microsoftedge.microsoft.com",
        prefix: &["addons", "detail"],
        store: Store::Edge,
    },
];

/// A theme parsed from a store URL.
pub struct ParsedStoreTheme {
    pub id: String,
    /// The link's slug as a title, or `None` when the link has no slug (the
    /// caller names it otherwise).
    pub name: Option<String>,
    pub store: Store,
}

/// A theme link from the Chrome Web Store or Edge Add-ons, or `None` for any
/// other text. Surrounding space, http, a missing scheme, `www.`, a trailing
/// slash, a query (such as `?hl=en`) and a fragment are all tolerated.
#[must_use]
pub fn parse(url: &str) -> Option<ParsedStoreTheme> {
    let trimmed = url.trim();
    let after_scheme = match trimmed.split_once("://") {
        Some((scheme, rest)) => {
            let is_web_link =
                scheme.eq_ignore_ascii_case("https") || scheme.eq_ignore_ascii_case("http");
            if !is_web_link {
                return None;
            }
            rest
        }
        None => trimmed,
    };
    let authority_end = after_scheme
        .find(['/', '?', '#'])
        .unwrap_or(after_scheme.len());
    let (authority, rest) = after_scheme.split_at(authority_end);
    let host = host_of(authority);
    let site = SITES
        .iter()
        .find(|site| site.host.eq_ignore_ascii_case(host))?;

    let path = rest.split(['?', '#']).next().unwrap_or("");
    let segments: Vec<&str> = path.split('/').filter(|s| !s.is_empty()).collect();
    let has_prefix = segments.len() > site.prefix.len()
        && segments
            .iter()
            .zip(site.prefix)
            .all(|(segment, expected)| segment.eq_ignore_ascii_case(expected));
    if !has_prefix {
        return None;
    }
    let rest = &segments[site.prefix.len()..];
    // /<id>, or /<slug>/<id>; a tab after it (/reviews) is ignored.
    if is_extension_id(rest[0]) {
        return Some(ParsedStoreTheme {
            id: rest[0].to_owned(),
            name: None,
            store: site.store,
        });
    }
    let has_slug_and_id = rest.len() > 1 && is_extension_id(rest[1]);
    if !has_slug_and_id {
        return None;
    }
    Some(ParsedStoreTheme {
        id: rest[1].to_owned(),
        name: Some(title_from_slug(rest[0])),
        store: site.store,
    })
}

// The host of `user@host:port`, without "www.".
fn host_of(authority: &str) -> &str {
    let host_and_port = authority.rsplit('@').next().unwrap_or(authority);
    let host = host_and_port.split(':').next().unwrap_or(host_and_port);
    let has_www =
        host.len() > WWW_PREFIX.len() && host[..WWW_PREFIX.len()].eq_ignore_ascii_case(WWW_PREFIX);
    if has_www {
        &host[WWW_PREFIX.len()..]
    } else {
        host
    }
}

// An extension id is 32 characters of a..p.
fn is_extension_id(value: &str) -> bool {
    value.len() == 32 && value.bytes().all(|byte| (b'a'..=b'p').contains(&byte))
}

// "dusk-at-sea" -> "Dusk At Sea".
fn title_from_slug(slug: &str) -> String {
    let spaced = slug.replace(['-', '_'], " ");
    let spaced = spaced.trim();
    if spaced.is_empty() {
        return slug.to_owned();
    }
    spaced
        .split(' ')
        .filter(|word| !word.is_empty())
        .map(capitalize)
        .collect::<Vec<_>>()
        .join(" ")
}

fn capitalize(word: &str) -> String {
    let mut chars = word.chars();
    match chars.next() {
        Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
        None => String::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const ID: &str = "abcdefghijklmnopabcdefghijklmnop";

    #[test]
    fn the_real_store_links_parse() {
        let accepted = [
            (
                format!(
                    "  https://chromewebstore.google.com/detail/dusk-at_sea/{ID}?hl=en#reviews "
                ),
                Store::Chrome,
                Some("Dusk At Sea"),
            ),
            (
                format!("https://chromewebstore.google.com/detail/{ID}"),
                Store::Chrome,
                None,
            ),
            (
                format!("https://chrome.google.com/webstore/detail/dusk/{ID}"),
                Store::Chrome,
                Some("Dusk"),
            ),
            (
                format!("https://chrome.google.com/webstore/detail/{ID}?hl=en"),
                Store::Chrome,
                None,
            ),
            (
                format!("https://microsoftedge.microsoft.com/addons/detail/night/{ID}"),
                Store::Edge,
                Some("Night"),
            ),
            (
                format!("https://microsoftedge.microsoft.com/addons/detail/{ID}"),
                Store::Edge,
                None,
            ),
            (
                format!("http://www.chromewebstore.google.com/detail/dusk/{ID}/"),
                Store::Chrome,
                Some("Dusk"),
            ),
            (
                format!("HTTPS://ChromeWebStore.Google.com/detail/dusk/{ID}#top"),
                Store::Chrome,
                Some("Dusk"),
            ),
            (
                format!("chromewebstore.google.com/detail/dusk/{ID}"),
                Store::Chrome,
                Some("Dusk"),
            ),
            (
                format!("https://chromewebstore.google.com/detail/dusk/{ID}/reviews"),
                Store::Chrome,
                Some("Dusk"),
            ),
        ];
        for (url, store, name) in accepted {
            let parsed = parse(&url).unwrap_or_else(|| panic!("{url} parses"));
            assert_eq!(parsed.id, ID, "{url}");
            assert_eq!(parsed.store, store, "{url}");
            assert_eq!(parsed.name.as_deref(), name, "{url} never names \"Detail\"");
        }
    }

    #[test]
    fn any_other_link_is_refused() {
        let refused = [
            format!("https://example.com/{ID}"),
            format!("https://example.com/detail/dusk/{ID}"),
            format!("https://chromewebstore.google.com.evil.example/detail/dusk/{ID}"),
            format!("https://notmicrosoftedge.microsoft.com/addons/detail/dusk/{ID}"),
            format!("https://microsoftedge.microsoft.com/detail/dusk/{ID}"),
            format!("https://chrome.google.com/detail/dusk/{ID}"),
            format!("ftp://chromewebstore.google.com/detail/x/{ID}"),
            "https://chromewebstore.google.com/detail/x/short".to_owned(),
            "https://chromewebstore.google.com/detail/dusk".to_owned(),
            String::new(),
        ];
        for url in refused {
            assert!(parse(&url).is_none(), "\"{url}\" is not a store link");
        }
        assert!(
            NOT_A_STORE_LINK.contains("Chrome Web Store")
                && NOT_A_STORE_LINK.contains("Edge Add-ons")
        );
    }
}
