//! Only the real stores are recognised, each by its exact host and the path
//! before the theme: <prefix>/<slug>/<id> or <prefix>/<id>. No network.

use crate::themes::package::extension_id;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ThemeStore {
    Chrome,
    Edge,
}

/// `name` is the link's slug as a title, or None when the link has none.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ParsedStoreTheme {
    pub id: String,
    pub name: Option<String>,
    pub store: ThemeStore,
}

const HTTPS_PREFIX: &str = "https://";
const SCHEME_SEPARATOR: &str = "://";
const WEB_SCHEMES: [&str; 2] = ["https", "http"];
const WWW_PREFIX: &str = "www.";
const PATH_SEPARATOR: char = '/';
const QUERY_START: char = '?';
const FRAGMENT_START: char = '#';
const USER_INFO_END: char = '@';
const PORT_SEPARATOR: char = ':';
const SLUG_SEPARATORS: [char; 2] = ['-', '_'];
const WORD_SEPARATOR: char = ' ';

/// `prefix` is the path segments in front of a theme's slug and id.
struct StoreSite {
    host: &'static str,
    prefix: &'static [&'static str],
    store: ThemeStore,
}

const SITES: [StoreSite; 3] = [
    StoreSite {
        host: "chromewebstore.google.com",
        prefix: &["detail"],
        store: ThemeStore::Chrome,
    },
    // The Chrome Web Store's old address, still in links people keep.
    StoreSite {
        host: "chrome.google.com",
        prefix: &["webstore", "detail"],
        store: ThemeStore::Chrome,
    },
    StoreSite {
        host: "microsoftedge.microsoft.com",
        prefix: &["addons", "detail"],
        store: ThemeStore::Edge,
    },
];

/// None for any other text. Surrounding space, http, a missing scheme, "www.",
/// a trailing slash, a query (such as ?hl=en) and a fragment are tolerated.
#[must_use]
pub fn parse(url: &str) -> Option<ParsedStoreTheme> {
    let trimmed = url.trim();
    let absolute = if trimmed.contains(SCHEME_SEPARATOR) {
        trimmed.to_owned()
    } else {
        format!("{HTTPS_PREFIX}{trimmed}")
    };
    let (scheme, rest) = absolute.split_once(SCHEME_SEPARATOR)?;
    let is_web_link = WEB_SCHEMES
        .iter()
        .any(|web| web.eq_ignore_ascii_case(scheme));
    if !is_web_link {
        return None;
    }
    let without_fragment = rest.split(FRAGMENT_START).next().unwrap_or_default();
    let without_query = without_fragment
        .split(QUERY_START)
        .next()
        .unwrap_or_default();
    let (authority, path) = without_query
        .split_once(PATH_SEPARATOR)
        .unwrap_or((without_query, ""));
    let host = host_of(authority);
    let has_www = host
        .get(..WWW_PREFIX.len())
        .is_some_and(|start| start.eq_ignore_ascii_case(WWW_PREFIX));
    let host = if has_www {
        &host[WWW_PREFIX.len()..]
    } else {
        host
    };
    let site = SITES
        .iter()
        .find(|each| each.host.eq_ignore_ascii_case(host))?;

    let segments: Vec<&str> = path
        .split(PATH_SEPARATOR)
        .filter(|segment| !segment.is_empty())
        .collect();
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
    if extension_id::is_valid(rest[0]) {
        return Some(ParsedStoreTheme {
            id: rest[0].to_owned(),
            name: None,
            store: site.store,
        });
    }
    let has_slug_and_id = rest.len() > 1 && extension_id::is_valid(rest[1]);
    if !has_slug_and_id {
        return None;
    }
    Some(ParsedStoreTheme {
        id: rest[1].to_owned(),
        name: Some(title_from_slug(rest[0])),
        store: site.store,
    })
}

/// The authority without its user info and port.
fn host_of(authority: &str) -> &str {
    let after_user = authority
        .rsplit_once(USER_INFO_END)
        .map_or(authority, |(_, host)| host);
    after_user
        .split_once(PORT_SEPARATOR)
        .map_or(after_user, |(host, _)| host)
}

/// "dusk-at-sea" -> "Dusk At Sea".
fn title_from_slug(slug: &str) -> String {
    let spaced = slug.replace(SLUG_SEPARATORS, &WORD_SEPARATOR.to_string());
    let spaced = spaced.trim();
    if spaced.is_empty() {
        return slug.to_owned();
    }
    spaced
        .split(WORD_SEPARATOR)
        .filter(|word| !word.is_empty())
        .map(capitalize)
        .collect::<Vec<_>>()
        .join(&WORD_SEPARATOR.to_string())
}

fn capitalize(word: &str) -> String {
    let mut letters = word.chars();
    letters.next().map_or_else(String::new, |first| {
        first.to_uppercase().chain(letters).collect()
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ui::wording;

    const THEME_ID: &str = "aaaabbbbccccddddeeeeffffgggghhhh";

    #[test]
    fn store_links_yield_id_name_and_store() {
        let chrome = parse(&format!(
            "  https://chromewebstore.google.com/detail/dusk-at_sea/{THEME_ID}?hl=en#reviews "
        ))
        .expect("a Chrome Web Store link parses");
        assert_eq!(THEME_ID, chrome.id, "the id is the 32-letter segment");
        assert_eq!(
            Some("Dusk At Sea"),
            chrome.name.as_deref(),
            "the slug becomes a title"
        );
        assert_eq!(
            ThemeStore::Chrome,
            chrome.store,
            "google's host is the Chrome store"
        );

        let edge = parse(&format!(
            "https://microsoftedge.microsoft.com/addons/detail/night/{THEME_ID}"
        ))
        .expect("an Edge Add-ons link parses");
        assert_eq!(
            ThemeStore::Edge,
            edge.store,
            "microsoft's host is Edge Add-ons"
        );
        assert_eq!(
            Some("Night"),
            edge.name.as_deref(),
            "one word is capitalized"
        );

        assert!(
            parse(&format!(
                "ftp://chromewebstore.google.com/detail/x/{THEME_ID}"
            ))
            .is_none(),
            "only http(s) links"
        );
        assert!(
            parse("https://chromewebstore.google.com/detail/x/short").is_none(),
            "a link with no id"
        );
    }

    #[test]
    fn store_links_are_the_real_stores_only() {
        let accepted = [
            (
                format!("https://chromewebstore.google.com/detail/{THEME_ID}"),
                ThemeStore::Chrome,
                None,
            ),
            (
                format!("https://chrome.google.com/webstore/detail/dusk/{THEME_ID}"),
                ThemeStore::Chrome,
                Some("Dusk"),
            ),
            (
                format!("https://chrome.google.com/webstore/detail/{THEME_ID}?hl=en"),
                ThemeStore::Chrome,
                None,
            ),
            (
                format!("https://microsoftedge.microsoft.com/addons/detail/{THEME_ID}"),
                ThemeStore::Edge,
                None,
            ),
            (
                format!("http://www.chromewebstore.google.com/detail/dusk/{THEME_ID}/"),
                ThemeStore::Chrome,
                Some("Dusk"),
            ),
            (
                format!("HTTPS://ChromeWebStore.Google.com/detail/dusk/{THEME_ID}#top"),
                ThemeStore::Chrome,
                Some("Dusk"),
            ),
            (
                format!("chromewebstore.google.com/detail/dusk/{THEME_ID}"),
                ThemeStore::Chrome,
                Some("Dusk"),
            ),
            (
                format!("https://chromewebstore.google.com/detail/dusk/{THEME_ID}/reviews"),
                ThemeStore::Chrome,
                Some("Dusk"),
            ),
        ];
        for (url, store, name) in accepted {
            let parsed = parse(&url).unwrap_or_else(|| panic!("{url} parses"));
            assert_eq!(THEME_ID, parsed.id, "{url} names the id");
            assert_eq!(store, parsed.store, "{url} is the {store:?} store");
            assert_eq!(
                name,
                parsed.name.as_deref(),
                "{url} names {name:?}, never \"Detail\""
            );
        }
        let refused = [
            format!("https://example.com/{THEME_ID}"),
            format!("https://example.com/detail/dusk/{THEME_ID}"),
            format!("https://chromewebstore.google.com.evil.example/detail/dusk/{THEME_ID}"),
            format!("https://notmicrosoftedge.microsoft.com/addons/detail/dusk/{THEME_ID}"),
            format!("https://microsoftedge.microsoft.com/detail/dusk/{THEME_ID}"),
            format!("https://chrome.google.com/detail/dusk/{THEME_ID}"),
            "https://chromewebstore.google.com/detail/dusk".to_owned(),
            String::new(),
        ];
        for url in refused {
            assert!(parse(&url).is_none(), "\"{url}\" is not a store link");
        }
        let names_both_stores = wording::NOT_A_STORE_LINK.contains("Chrome Web Store")
            && wording::NOT_A_STORE_LINK.contains("Edge Add-ons");
        assert!(names_both_stores, "the refusal names both stores");
    }
}
