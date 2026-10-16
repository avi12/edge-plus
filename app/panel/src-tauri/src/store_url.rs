//! Pulling a theme's identity out of a pasted store URL.
//!
//! Edge Add-ons and the Chrome Web Store both end `/detail/<slug>/<id>`, where
//! `<id>` is 32 characters of `a..p`. Mirrors `TryParseStoreUrl` in the old
//! `ThemeInstaller.cs`; pure string logic, no network.

use edge_native::Store;

/// A theme parsed from a store URL.
pub struct ParsedStoreTheme {
    pub id: String,
    pub name: String,
    pub store: Store,
}

/// Parse a Chrome Web Store / Edge Add-ons URL, or `None` if it carries no
/// extension id.
#[must_use]
pub fn parse(url: &str) -> Option<ParsedStoreTheme> {
    let trimmed = url.trim();
    let after_scheme = trimmed
        .strip_prefix("https://")
        .or_else(|| trimmed.strip_prefix("http://"))?;
    let (host, path) = after_scheme.split_once('/').unwrap_or((after_scheme, ""));
    let path = path.split(['?', '#']).next().unwrap_or("");
    let segments: Vec<&str> = path.split('/').filter(|s| !s.is_empty()).collect();

    let id_index = segments
        .iter()
        .rposition(|segment| is_extension_id(segment))?;
    let id = segments[id_index].to_owned();
    let slug = if id_index > 0 {
        segments[id_index - 1]
    } else {
        &id
    };
    let name = title_from_slug(slug);
    let store = if host.to_ascii_lowercase().contains("microsoft") {
        Store::Edge
    } else {
        Store::Chrome
    };
    Some(ParsedStoreTheme { id, name, store })
}

// An extension id is 32 characters of a..p.
fn is_extension_id(value: &str) -> bool {
    value.len() == 32 && value.bytes().all(|byte| (b'a'..=b'p').contains(&byte))
}

fn title_from_slug(slug: &str) -> String {
    let spaced = slug.replace(['-', '_'], " ");
    let spaced = spaced.trim();
    if spaced.is_empty() {
        return slug.to_owned();
    }
    spaced
        .split(' ')
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
