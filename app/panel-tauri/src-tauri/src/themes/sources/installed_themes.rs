//! Read-only, straight from Edge's profiles on disk: the DLL applies an
//! installed theme by id exactly like one the panel installed.

use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};

use serde_json::Value;

use crate::edge::edge_user_data::{PREFERENCES_FILE, read_json};
use crate::themes::package::extension_id;
use crate::themes::package::theme_manifest::{MANIFEST_NAME, manifest_keys};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct InstalledTheme {
    pub id: String,
    pub name: String,
    pub is_active: bool,
}

const MESSAGE_PREFIX: &str = "__MSG_";
const MESSAGE_SUFFIX: &str = "__";
const MESSAGE_KEY: &str = "message";
const FALLBACK_LOCALE: &str = "en";
const EXTENSIONS_FOLDER: &str = "Extensions";
const LOCALES_FOLDER: &str = "_locales";
const MESSAGES_FILE: &str = "messages.json";
const SECURE_PREFERENCES_FILE: &str = "Secure Preferences";
const VERSION_FOLDER_SEPARATOR: char = '_';
const VERSION_SEPARATOR: char = '.';
/// The JSON path, in Preferences, to the id of the theme in use.
const ACTIVE_THEME_PATH: [&str; 3] = ["extensions", "theme", "id"];
/// The JSON path, in Secure Preferences, to the object keyed by every
/// registered extension's id.
const REGISTERED_PATH: [&str; 2] = ["extensions", "settings"];

/// None when the manifest is not a theme's. `messages` resolves a
/// `__MSG_key__` name.
#[must_use]
pub fn theme_name(manifest: Option<&Value>, messages: Option<&Value>) -> Option<String> {
    let manifest = manifest?;
    let is_theme = manifest
        .get(manifest_keys::THEME)
        .is_some_and(Value::is_object);
    let name = manifest.get(manifest_keys::NAME).and_then(Value::as_str);
    let name = name.filter(|_| is_theme)?;
    Some(resolve_message(name, messages))
}

/// Keys match ignoring case, as Chromium does.
#[must_use]
pub fn resolve_message(text: &str, messages: Option<&Value>) -> String {
    let key = text
        .strip_prefix(MESSAGE_PREFIX)
        .and_then(|rest| rest.strip_suffix(MESSAGE_SUFFIX));
    let entries = messages.and_then(Value::as_object);
    let (Some(key), Some(entries)) = (key, entries) else {
        return text.to_owned();
    };
    let key = key.to_lowercase();
    let message = entries
        .iter()
        .filter(|(name, _)| name.to_lowercase() == key)
        .find_map(|(_, entry)| entry.get(MESSAGE_KEY).and_then(Value::as_str));
    message.unwrap_or(text).to_owned()
}

#[must_use]
pub fn active_theme_id(preferences: Option<&Value>) -> Option<String> {
    let id = ACTIVE_THEME_PATH
        .iter()
        .try_fold(preferences?, |node, key| node.get(key))?;
    id.as_str().map(ToOwned::to_owned)
}

/// So a folder left behind by an uninstall is not listed; None when unknown.
#[must_use]
pub fn registered_ids(secure_preferences: Option<&Value>) -> Option<HashSet<String>> {
    let settings = REGISTERED_PATH
        .iter()
        .try_fold(secure_preferences?, |node, key| node.get(key))?;
    Some(settings.as_object()?.keys().cloned().collect())
}

/// Each id once, active if it is active in any profile, ordered by name.
#[must_use]
pub fn scan(user_data: &Path) -> Vec<InstalledTheme> {
    let mut found: Vec<InstalledTheme> = Vec::new();
    for profile in profile_folders(user_data) {
        for theme in scan_profile(&profile) {
            match found.iter_mut().find(|existing| existing.id == theme.id) {
                None => found.push(theme),
                Some(known) => known.is_active |= theme.is_active,
            }
        }
    }
    found.sort_by_cached_key(|theme| theme.name.to_lowercase());
    found
}

/// `1.10.0_0` sorts after `1.9.0_0`: the dotted numbers, not the text.
#[must_use]
pub fn version_key(folder: &Path) -> Vec<i64> {
    let name = folder
        .file_name()
        .map(|name| name.to_string_lossy().into_owned())
        .unwrap_or_default();
    let version = name
        .split(VERSION_FOLDER_SEPARATOR)
        .next()
        .unwrap_or_default();
    version
        .split(VERSION_SEPARATOR)
        .map(|part| part.parse().unwrap_or(0))
        .collect()
}

fn sub_folders(folder: &Path) -> Option<Vec<PathBuf>> {
    let entries = fs::read_dir(folder).ok()?;
    Some(
        entries
            .filter_map(Result::ok)
            .map(|entry| entry.path())
            .filter(|path| path.is_dir())
            .collect(),
    )
}

/// Default, Profile 1, ...: every folder with an Extensions folder.
fn profile_folders(user_data: &Path) -> Vec<PathBuf> {
    sub_folders(user_data)
        .unwrap_or_default()
        .into_iter()
        .filter(|folder| folder.join(EXTENSIONS_FOLDER).is_dir())
        .collect()
}

fn scan_profile(profile: &Path) -> Vec<InstalledTheme> {
    let active = active_theme_id(read_json(&profile.join(PREFERENCES_FILE)).as_ref());
    let registered = registered_ids(read_json(&profile.join(SECURE_PREFERENCES_FILE)).as_ref());
    let Some(extensions) = sub_folders(&profile.join(EXTENSIONS_FOLDER)) else {
        return Vec::new();
    };
    let mut themes = Vec::new();
    for extension in extensions {
        let id = extension
            .file_name()
            .map(|name| name.to_string_lossy().into_owned())
            .unwrap_or_default();
        let is_registered = registered.as_ref().is_none_or(|ids| ids.contains(&id));
        let is_listable = extension_id::is_valid(&id) && is_registered;
        if !is_listable {
            continue;
        }
        let name = newest_version_folder(&extension).and_then(|version| theme_name_in(&version));
        let Some(name) = name else {
            continue;
        };
        let is_active = active.as_deref() == Some(id.as_str());
        themes.push(InstalledTheme {
            id,
            name,
            is_active,
        });
    }
    themes
}

/// The last of equal versions wins, as the folders are listed.
fn newest_version_folder(extension: &Path) -> Option<PathBuf> {
    sub_folders(extension)?
        .into_iter()
        .filter(|folder| folder.join(MANIFEST_NAME).is_file())
        .map(|folder| (version_key(&folder), folder))
        // Part by part, then the longer is the newer.
        .max_by(|(left, _), (right, _)| left.cmp(right))
        .map(|(_, folder)| folder)
}

fn theme_name_in(version_folder: &Path) -> Option<String> {
    let manifest = read_json(&version_folder.join(MANIFEST_NAME));
    let messages = read_messages(version_folder, manifest.as_ref());
    theme_name(manifest.as_ref(), messages.as_ref())
}

/// The _locales/<locale>/messages.json for the manifest's default locale, else
/// English, else whichever locale exists.
fn read_messages(version_folder: &Path, manifest: Option<&Value>) -> Option<Value> {
    let locales = version_folder.join(LOCALES_FOLDER);
    let default_locale = manifest
        .and_then(|manifest| manifest.get(manifest_keys::DEFAULT_LOCALE))
        .and_then(Value::as_str);
    for preferred in [default_locale, Some(FALLBACK_LOCALE)]
        .into_iter()
        .flatten()
    {
        let messages = read_json(&locales.join(preferred).join(MESSAGES_FILE));
        if messages.is_some() {
            return messages;
        }
    }
    let first = sub_folders(&locales)?.into_iter().next()?;
    read_json(&first.join(MESSAGES_FILE))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn json(text: &str) -> Value {
        serde_json::from_str(text).expect("json")
    }

    #[test]
    fn names_come_only_from_themes() {
        assert_eq!(
            Some("Dusk".to_owned()),
            theme_name(Some(&json(r#"{"name":"Dusk","theme":{}}"#)), None),
            "a theme's plain name"
        );
        assert_eq!(
            None,
            theme_name(Some(&json(r#"{"name":"uBlock"}"#)), None),
            "an extension without a theme key is not a theme"
        );
    }

    #[test]
    fn localised_names_resolve_ignoring_key_case() {
        let messages = json(r#"{"APPNAME":{"message":"Sunset"}}"#);
        assert_eq!(
            Some("Sunset".to_owned()),
            theme_name(
                Some(&json(r#"{"name":"__MSG_appName__","theme":{}}"#)),
                Some(&messages)
            ),
            "a __MSG_ name resolves"
        );
        assert_eq!(
            "__MSG_gone__",
            resolve_message("__MSG_gone__", None),
            "no messages keeps the placeholder"
        );
        assert_eq!(
            "__MSG_gone__",
            resolve_message("__MSG_gone__", Some(&json(r#"{"x":{"message":"y"}}"#))),
            "an unknown key keeps the placeholder"
        );
    }

    #[test]
    fn the_active_theme_and_the_registered_ids_are_read() {
        assert_eq!(
            Some("abc".to_owned()),
            active_theme_id(Some(&json(r#"{"extensions":{"theme":{"id":"abc"}}}"#))),
            "the active id"
        );
        assert_eq!(
            None,
            active_theme_id(Some(&json(r#"{"extensions":{}}"#))),
            "no theme in use"
        );
        let ids = registered_ids(Some(&json(
            r#"{"extensions":{"settings":{"a":{},"b":{}}}}"#,
        )))
        .expect("both ids");
        let has_both = ids.len() == 2 && ids.contains("a") && ids.contains("b");
        assert!(has_both, "both ids");
        assert_eq!(
            None,
            registered_ids(Some(&json("{}"))),
            "unknown is not empty"
        );
    }

    #[test]
    fn versions_compare_numerically() {
        let older = version_key(Path::new(r"x\1.9.0_0"));
        let newer = version_key(Path::new(r"x\1.10.0_1"));
        let is_numeric = older[1] == 9 && newer[1] == 10;
        assert!(is_numeric, "1.10 is ten, not one-dot-one-oh");
        assert!(older < newer);
        assert!(vec![1, 0] < vec![1, 0, 0], "longer is newer");
    }

    // Needs edge_user_data::read_json, which another part of the rewrite fills.
    #[test]
    fn a_scan_lists_installed_themes_and_marks_the_active_one() {
        let root = std::env::temp_dir().join(format!("edgeplus-themes-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let profile = root.join("Default");
        let id = "a".repeat(extension_id::LENGTH);
        let other = "b".repeat(extension_id::LENGTH);
        let version = profile.join(EXTENSIONS_FOLDER).join(&id).join("1.0_0");
        let locale = version.join(LOCALES_FOLDER).join("en");
        fs::create_dir_all(&locale).expect("folders");
        // With a byte-order mark, as Edge writes some of these.
        fs::write(
            version.join(MANIFEST_NAME),
            "\u{feff}{\"name\":\"__MSG_n__\",\"default_locale\":\"en\",\"theme\":{}}",
        )
        .expect("manifest");
        fs::write(locale.join(MESSAGES_FILE), r#"{"n":{"message":"Night"}}"#).expect("messages");
        let plain = profile.join(EXTENSIONS_FOLDER).join(&other).join("2.0_0");
        fs::create_dir_all(&plain).expect("folder");
        fs::write(plain.join(MANIFEST_NAME), r#"{"name":"Tool"}"#).expect("manifest");
        fs::write(
            profile.join(PREFERENCES_FILE),
            format!(r#"{{"extensions":{{"theme":{{"id":"{id}"}}}}}}"#),
        )
        .expect("preferences");

        let themes = scan(&root);
        fs::remove_dir_all(&root).expect("clean up");
        assert_eq!(1, themes.len(), "only the theme is listed");
        assert_eq!(
            InstalledTheme {
                id,
                name: "Night".to_owned(),
                is_active: true
            },
            themes[0],
            "named from its locale and marked in use"
        );
    }
}
