//! A theme is added without touching Edge's ABI: an entry under Edge's
//! per-user external-extensions key, and Edge's loader installs it. A store
//! theme points at the store's update URL, so Edge trusts it without developer
//! mode.

use std::io;
use std::path::Path;

use edge_native::Hive;

use crate::themes::sources::store_url::ThemeStore;

const EXTENSIONS_KEY: &str = r"Software\Microsoft\Edge\Extensions";
const CHROME_STORE_UPDATE_URL: &str = "https://clients2.google.com/service/update2/crx";
const EDGE_STORE_UPDATE_URL: &str = "https://edge.microsoft.com/extensionwebstorebase/v1/crx";
// The value names Edge's loader reads under each id.
const UPDATE_URL_VALUE: &str = "update_url";
const PATH_VALUE: &str = "path";
const VERSION_VALUE: &str = "version";

/// The store's update endpoint, which Edge installs from and which also serves
/// the theme's package.
#[must_use]
pub(crate) fn update_url(store: ThemeStore) -> &'static str {
    match store {
        ThemeStore::Edge => EDGE_STORE_UPDATE_URL,
        ThemeStore::Chrome => CHROME_STORE_UPDATE_URL,
    }
}

pub(crate) fn install_store(extension_id: &str, store: ThemeStore) -> io::Result<()> {
    edge_native::write_string(
        Hive::CurrentUser,
        &key_path(extension_id),
        UPDATE_URL_VALUE,
        update_url(store),
    )
}

pub(crate) fn install_local(extension_id: &str, crx_path: &Path, version: &str) -> io::Result<()> {
    let key = key_path(extension_id);
    edge_native::write_string(
        Hive::CurrentUser,
        &key,
        PATH_VALUE,
        &crx_path.to_string_lossy(),
    )?;
    edge_native::write_string(Hive::CurrentUser, &key, VERSION_VALUE, version)
}

/// Edge removes the extension on its next start.
pub(crate) fn uninstall(extension_id: &str) -> io::Result<()> {
    match edge_native::delete_tree(Hive::CurrentUser, &key_path(extension_id)) {
        Err(error) if error.kind() == io::ErrorKind::PermissionDenied => Ok(()),
        result => result,
    }
}

/// Whether Edge installed the extension from this key (location 3 in its
/// Secure Preferences), so deleting the key uninstalls it. One installed from a
/// store inside Edge has no key.
#[must_use]
pub(crate) fn is_registered(extension_id: &str) -> bool {
    let key = key_path(extension_id);
    [UPDATE_URL_VALUE, PATH_VALUE]
        .iter()
        .any(|value| edge_native::read_string(Hive::CurrentUser, &key, Some(value)).is_some())
}

fn key_path(extension_id: &str) -> String {
    format!(r"{EXTENSIONS_KEY}\{extension_id}")
}
