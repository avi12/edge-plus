//! The themes Edge already has installed (from Edge Add-ons or the Chrome Web
//! Store), read straight from a profile on disk, read-only. A theme is an
//! extension whose manifest has a `theme` key; the DLL applies it by id exactly
//! like one the panel installed, so nothing here installs or writes anything.

use std::collections::HashSet;
use std::path::{Path, PathBuf};

use serde::Serialize;
use serde_json::Value;

const MESSAGE_PREFIX: &str = "__MSG_";
const MESSAGE_SUFFIX: &str = "__";
const FALLBACK_LOCALE: &str = "en";
const EXTENSION_ID_LENGTH: usize = 32;

/// A theme Edge has installed in one of its profiles.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct InstalledTheme {
    pub id: String,
    pub name: String,
    /// The theme Edge is showing right now in that profile.
    pub is_active: bool,
}

/// The name of the theme a manifest describes, or `None` when the manifest is
/// not a theme. `messages` is the manifest's resolved `messages.json`, used for
/// a `__MSG_key__` name.
#[must_use]
pub fn theme_name(manifest: &Value, messages: Option<&Value>) -> Option<String> {
    let is_theme = manifest.get("theme").is_some_and(Value::is_object);
    if !is_theme {
        return None;
    }
    let raw = manifest.get("name")?.as_str()?;
    Some(resolve_message(raw, messages))
}

/// A `__MSG_key__` placeholder looked up in `messages` (keys are matched
/// case-insensitively, like Chromium does); anything else is returned as is.
#[must_use]
pub fn resolve_message(text: &str, messages: Option<&Value>) -> String {
    let key = text
        .strip_prefix(MESSAGE_PREFIX)
        .and_then(|rest| rest.strip_suffix(MESSAGE_SUFFIX));
    let Some(key) = key else {
        return text.to_owned();
    };
    let found = messages
        .and_then(Value::as_object)
        .and_then(|entries| {
            entries
                .iter()
                .find(|(name, _)| name.eq_ignore_ascii_case(key))
        })
        .and_then(|(_, entry)| entry.get("message"))
        .and_then(Value::as_str);
    found.unwrap_or(text).to_owned()
}

/// The id of the theme a profile's `Preferences` says is in use.
#[must_use]
pub fn active_theme_id(preferences: &Value) -> Option<String> {
    let id = preferences.get("extensions")?.get("theme")?.get("id")?;
    id.as_str().map(str::to_owned)
}

/// The extension ids a profile's `Secure Preferences` still registers, so a
/// folder left behind by an uninstall is not listed.
#[must_use]
pub fn registered_ids(secure_preferences: &Value) -> Option<HashSet<String>> {
    let settings = secure_preferences
        .get("extensions")?
        .get("settings")?
        .as_object()?;
    Some(settings.keys().cloned().collect())
}

/// Every installed theme across all profiles under `user_data`, each id once
/// (active if it is active in any profile), sorted by name.
#[must_use]
pub fn scan(user_data: &Path) -> Vec<InstalledTheme> {
    let mut found: Vec<InstalledTheme> = Vec::new();
    for profile in profile_dirs(user_data) {
        for theme in scan_profile(&profile) {
            let existing = found.iter_mut().find(|known| known.id == theme.id);
            match existing {
                Some(known) => known.is_active |= theme.is_active,
                None => found.push(theme),
            }
        }
    }
    found.sort_by_cached_key(|theme| theme.name.to_lowercase());
    found
}

// `Default`, `Profile 1`, ... : every folder with an `Extensions` folder.
fn profile_dirs(user_data: &Path) -> Vec<PathBuf> {
    let Ok(entries) = std::fs::read_dir(user_data) else {
        return Vec::new();
    };
    entries
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.join("Extensions").is_dir())
        .collect()
}

fn scan_profile(profile: &Path) -> Vec<InstalledTheme> {
    let active = read_json(&profile.join("Preferences")).and_then(|prefs| active_theme_id(&prefs));
    let registered =
        read_json(&profile.join("Secure Preferences")).and_then(|secure| registered_ids(&secure));
    let Ok(entries) = std::fs::read_dir(profile.join("Extensions")) else {
        return Vec::new();
    };
    let mut themes = Vec::new();
    for entry in entries.filter_map(Result::ok) {
        let id = entry.file_name().to_string_lossy().into_owned();
        let is_registered = registered.as_ref().is_none_or(|ids| ids.contains(&id));
        if !is_extension_id(&id) || !is_registered {
            continue;
        }
        let Some(name) = newest_version_dir(&entry.path()).and_then(|dir| theme_name_in(&dir))
        else {
            continue;
        };
        themes.push(InstalledTheme {
            is_active: active.as_deref() == Some(id.as_str()),
            id,
            name,
        });
    }
    themes
}

// The highest `<version>_<n>` folder of an extension.
fn newest_version_dir(extension: &Path) -> Option<PathBuf> {
    let mut versions: Vec<PathBuf> = std::fs::read_dir(extension)
        .ok()?
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.join("manifest.json").is_file())
        .collect();
    versions.sort_by_cached_key(|path| version_key(path));
    versions.pop()
}

// `1.10.0_0` sorts after `1.9.0_0`: compare the dotted numbers, not the text.
fn version_key(path: &Path) -> Vec<u64> {
    let name = path
        .file_name()
        .map(|name| name.to_string_lossy().into_owned())
        .unwrap_or_default();
    name.split('_')
        .next()
        .unwrap_or("")
        .split('.')
        .map(|part| part.parse().unwrap_or(0))
        .collect()
}

fn theme_name_in(version_dir: &Path) -> Option<String> {
    let manifest = read_json(&version_dir.join("manifest.json"))?;
    let messages = read_messages(version_dir, &manifest);
    theme_name(&manifest, messages.as_ref())
}

// The `_locales/<locale>/messages.json` for the manifest's default locale,
// else English, else whichever locale exists.
fn read_messages(version_dir: &Path, manifest: &Value) -> Option<Value> {
    let locales = version_dir.join("_locales");
    let default_locale = manifest.get("default_locale").and_then(Value::as_str);
    let preferred = default_locale
        .into_iter()
        .chain(std::iter::once(FALLBACK_LOCALE));
    for locale in preferred {
        if let Some(messages) = read_json(&locales.join(locale).join("messages.json")) {
            return Some(messages);
        }
    }
    let first = std::fs::read_dir(&locales).ok()?.find_map(Result::ok)?;
    read_json(&first.path().join("messages.json"))
}

// Edge writes these files with a UTF-8 byte-order mark in some cases.
fn read_json(path: &Path) -> Option<Value> {
    let text = std::fs::read_to_string(path).ok()?;
    serde_json::from_str(text.trim_start_matches('\u{feff}')).ok()
}

fn is_extension_id(value: &str) -> bool {
    value.len() == EXTENSION_ID_LENGTH && value.bytes().all(|byte| (b'a'..=b'p').contains(&byte))
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn plain_name_of_a_theme() {
        let manifest = json!({ "name": "Dusk", "theme": { "colors": {} } });
        assert_eq!(theme_name(&manifest, None), Some("Dusk".to_owned()));
    }

    #[test]
    fn an_extension_without_a_theme_key_is_not_a_theme() {
        let manifest = json!({ "name": "uBlock", "permissions": [] });
        assert_eq!(theme_name(&manifest, None), None);
    }

    #[test]
    fn localised_name_is_resolved_ignoring_key_case() {
        let manifest = json!({ "name": "__MSG_appName__", "theme": {} });
        let messages = json!({ "APPNAME": { "message": "Sunset" } });
        assert_eq!(
            theme_name(&manifest, Some(&messages)),
            Some("Sunset".to_owned())
        );
    }

    #[test]
    fn unresolvable_placeholder_is_kept() {
        assert_eq!(resolve_message("__MSG_gone__", None), "__MSG_gone__");
        let messages = json!({ "other": { "message": "x" } });
        assert_eq!(
            resolve_message("__MSG_gone__", Some(&messages)),
            "__MSG_gone__"
        );
    }

    #[test]
    fn active_theme_comes_from_the_preferences_theme_id() {
        let preferences = json!({ "extensions": { "theme": { "id": "abc", "pack": "p" } } });
        assert_eq!(active_theme_id(&preferences), Some("abc".to_owned()));
        assert_eq!(active_theme_id(&json!({ "extensions": {} })), None);
    }

    #[test]
    fn registered_ids_are_the_extension_settings_keys() {
        let secure = json!({ "extensions": { "settings": { "a": {}, "b": {} } } });
        let ids = registered_ids(&secure).unwrap_or_default();
        assert!(ids.contains("a") && ids.contains("b") && ids.len() == 2);
        assert!(registered_ids(&json!({})).is_none());
    }

    #[test]
    fn versions_compare_numerically() {
        let older = version_key(Path::new("x/1.9.0_0"));
        let newer = version_key(Path::new("x/1.10.0_1"));
        assert!(older < newer);
    }

    #[test]
    fn scan_lists_installed_themes_and_marks_the_active_one() {
        let root = std::env::temp_dir().join(format!("fixedge-themes-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        let profile = root.join("Default");
        let id = "a".repeat(32);
        let other = "b".repeat(32);
        let version = profile.join("Extensions").join(&id).join("1.0_0");
        let locale = version.join("_locales").join("en");
        std::fs::create_dir_all(&locale).expect("test fixture");
        std::fs::write(
            version.join("manifest.json"),
            "\u{feff}{\"name\":\"__MSG_n__\",\"default_locale\":\"en\",\"theme\":{}}",
        )
        .expect("test fixture");
        std::fs::write(
            locale.join("messages.json"),
            "{\"n\":{\"message\":\"Night\"}}",
        )
        .expect("test fixture");
        let plain = profile.join("Extensions").join(&other).join("2.0_0");
        std::fs::create_dir_all(&plain).expect("test fixture");
        std::fs::write(plain.join("manifest.json"), "{\"name\":\"Tool\"}").expect("test fixture");
        std::fs::write(
            profile.join("Preferences"),
            format!("{{\"extensions\":{{\"theme\":{{\"id\":\"{id}\"}}}}}}"),
        )
        .expect("test fixture");

        let themes = scan(&root);
        let _ = std::fs::remove_dir_all(&root);
        assert_eq!(
            themes,
            vec![InstalledTheme {
                id,
                name: "Night".to_owned(),
                is_active: true
            }]
        );
    }
}
