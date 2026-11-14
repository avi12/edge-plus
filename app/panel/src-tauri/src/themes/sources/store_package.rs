//! A store theme's own package, fetched from the store's update endpoint (the
//! one Edge installs from) to read the theme's name out of its manifest, so the
//! panel does not wait for Edge to install it, nor use the link's slug, which
//! the store may cut short.

use std::time::Duration;

use crate::themes::package::crx;
use crate::themes::sources::external_extensions;
use crate::themes::sources::store_url::ThemeStore;

const REQUEST_TIMEOUT: Duration = Duration::from_secs(10);
const MAX_PACKAGE_BYTES: u64 = 64 * 1024 * 1024;
/// The Chromium the store is told this is; a store serves a package to any
/// recent browser.
const PRODUCT_VERSION: &str = "150.0";

/// The theme's name from its manifest; None when the store cannot be reached or
/// has no such package.
#[must_use]
pub(crate) fn name(id: &str, store: ThemeStore) -> Option<String> {
    let bytes = download(id, store)?;
    crx::read_zip_name(crx::zip(&bytes).ok()?)
}

fn download(id: &str, store: ThemeStore) -> Option<Vec<u8>> {
    let agent: ureq::Agent = ureq::Agent::config_builder()
        .timeout_global(Some(REQUEST_TIMEOUT))
        .build()
        .into();
    let url = format!(
        "{}?response=redirect&prodversion={PRODUCT_VERSION}&acceptformat=crx3&x=id%3D{id}%26installsource%3Dondemand%26uc",
        external_extensions::update_url(store)
    );
    agent
        .get(&url)
        .call()
        .ok()?
        .body_mut()
        .with_config()
        .limit(MAX_PACKAGE_BYTES)
        .read_to_vec()
        .ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Reaches the Chrome Web Store: `cargo test -- --ignored`.
    #[test]
    #[ignore = "uses the network"]
    fn a_store_theme_names_itself() {
        let name = name("cedciagceibcmcdbdecocoeccdklidpb", ThemeStore::Chrome);
        assert_eq!(Some("Cyberpunk Edgerunners Theme".to_owned()), name);
    }
}
