//! Snoozes Edge's "turn off extensions in developer mode" prompt. Edge rewrites
//! Preferences from memory while it runs, so the edit only sticks once Edge has
//! fully exited (exactly when the prompt is next checked). The waiting runs in
//! a headless re-run of the exe so it survives the window closing.

use std::fs::{self, OpenOptions};
use std::os::windows::fs::OpenOptionsExt;
use std::path::Path;
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use serde_json::{Map, Value};

use crate::edge::edge_user_data;
use crate::platform::identity::switches;
use crate::platform::settings::Settings;

/// One waiter at a time (every switch-on starts one).
const WAITER_MUTEX: &str = r"Local\edgeplus.dev-mode-prompt";
const LOCKFILE_NAME: &str = "lockfile";
const TEMPORARY_EXTENSION: &str = "edgeplus-tmp";
const SNOOZE_KEY_PATH: [&str; 3] = ["extensions", "ui", "dev_mode_warning_snooze_end_time"];
/// A hundred years: "never ask again" in practice.
const SNOOZE_MICROSECONDS: u64 = 100 * 365 * 24 * 60 * 60 * 1_000_000;
/// From Edge's epoch (1601-01-01, a FILETIME's) to the Unix one.
const SECONDS_FROM_1601_TO_1970: u64 = 11_644_473_600;
const POLL_INTERVAL: Duration = Duration::from_secs(2);
/// No sharing at all: the open fails while any other process holds the file.
const SHARE_NONE: u32 = 0;

#[must_use]
pub fn waiter_command_line(executable: &Path) -> String {
    format!("\"{}\" {}", executable.display(), switches::DEV_MODE_WAITER)
}

pub fn wait_for_edge_exit_then_snooze(user_data: &Path) {
    let Some(_waiter) = edge_native::NamedMutex::acquire_sole(WAITER_MUTEX) else {
        return;
    };
    while is_in_use(user_data) {
        thread::sleep(POLL_INTERVAL);
    }
    if !Settings::load().features.quiet_dev_mode {
        return;
    }
    let snooze_end = snooze_end(SystemTime::now());
    for profile in profile_folders(user_data) {
        let preferences = user_data
            .join(profile)
            .join(edge_user_data::PREFERENCES_FILE);
        write_snooze(&preferences, &snooze_end);
    }
}

/// In Edge's units: microseconds since 1601, as text.
#[must_use]
pub fn snooze_end(now: SystemTime) -> String {
    (microseconds_since_1601(now) + SNOOZE_MICROSECONDS).to_string()
}

fn microseconds_since_1601(now: SystemTime) -> u64 {
    let epoch_1601 = UNIX_EPOCH - Duration::from_secs(SECONDS_FROM_1601_TO_1970);
    now.duration_since(epoch_1601).map_or(0, |elapsed| {
        u64::try_from(elapsed.as_micros()).unwrap_or(u64::MAX)
    })
}

/// Creates the objects on the path, replacing any that are not objects.
pub fn set_snooze(preferences: &mut Map<String, Value>, snooze_end: &str) {
    let [parent_keys @ .., last_key] = SNOOZE_KEY_PATH;
    let mut parent = preferences;
    for key in parent_keys {
        let child = parent
            .entry(key)
            .or_insert_with(|| Value::Object(Map::new()));
        if !child.is_object() {
            *child = Value::Object(Map::new());
        }
        let Value::Object(child) = child else {
            unreachable!("made an object just above");
        };
        parent = child;
    }
    parent.insert(last_key.to_owned(), Value::String(snooze_end.to_owned()));
}

/// Edge holds User Data\lockfile open, unshared, for its whole run, so failing
/// to open it exclusively means Edge is still up.
fn is_in_use(user_data: &Path) -> bool {
    let lockfile = user_data.join(LOCKFILE_NAME);
    if !lockfile.is_file() {
        return false;
    }
    OpenOptions::new()
        .read(true)
        .share_mode(SHARE_NONE)
        .open(&lockfile)
        .is_err()
}

fn write_snooze(preferences: &Path, snooze_end: &str) {
    let Some(Value::Object(mut root)) = edge_user_data::read_json(preferences) else {
        return;
    };
    set_snooze(&mut root, snooze_end);
    let Ok(text) = serde_json::to_string(&root) else {
        return;
    };
    let temporary = preferences.with_extension(TEMPORARY_EXTENSION);
    if fs::write(&temporary, text).is_ok() {
        let _ = fs::rename(&temporary, preferences);
    }
}

/// Local State lists every profile Edge knows, by folder name.
fn profile_folders(user_data: &Path) -> Vec<String> {
    let local_state = edge_user_data::read_json(&user_data.join(edge_user_data::LOCAL_STATE_FILE));
    local_state
        .as_ref()
        .and_then(|state| state.get("profile"))
        .and_then(|profile| profile.get("info_cache"))
        .and_then(Value::as_object)
        .map(|profiles| profiles.keys().cloned().collect())
        .unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn snooze_at(preferences: &Value) -> Option<&str> {
        preferences
            .get("extensions")?
            .get("ui")?
            .get("dev_mode_warning_snooze_end_time")?
            .as_str()
    }

    #[test]
    fn the_snooze_ends_a_hundred_years_on_in_microseconds_since_1601() {
        let epoch_1601 = UNIX_EPOCH - Duration::from_secs(SECONDS_FROM_1601_TO_1970);
        let hundred_years: u64 = 100 * 365 * 24 * 60 * 60 * 1_000_000;
        assert_eq!(hundred_years.to_string(), snooze_end(epoch_1601));
    }

    #[test]
    fn the_snooze_lands_on_its_path_replacing_what_is_not_an_object() {
        let mut preferences: Value =
            serde_json::from_str(r#"{"keep":1,"extensions":{"ui":"not an object","settings":{}}}"#)
                .expect("parses");
        let Value::Object(root) = &mut preferences else {
            panic!("an object");
        };
        set_snooze(root, "42");
        assert_eq!(Some("42"), snooze_at(&preferences));
        assert!(
            preferences["extensions"]["settings"].is_object(),
            "siblings are kept"
        );
        assert_eq!(
            Some(1),
            preferences["keep"].as_i64(),
            "the rest of the file is kept"
        );
    }

    #[test]
    fn the_path_is_created_when_missing() {
        let mut empty = Map::new();
        set_snooze(&mut empty, "7");
        assert_eq!(Some("7"), snooze_at(&Value::Object(empty)));
    }

    #[test]
    fn the_waiter_is_this_exe_quoted_with_its_switch() {
        assert_eq!(
            "\"C:\\Program Files\\Edge+\\edgeplus.exe\" --quiet-dev-mode-after-exit",
            waiter_command_line(Path::new(r"C:\Program Files\Edge+\edgeplus.exe"))
        );
    }
}
