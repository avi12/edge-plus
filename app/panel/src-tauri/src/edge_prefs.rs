//! What the panel writes into each Edge profile's `Preferences` while Edge is
//! closed (`edge_native::edit_every_profile` does the waiting and the file
//! swap): the developer-mode prompt's snooze, and the web protocol handlers -
//! the same entries Edge writes when a site asks to open `mailto:` links and the
//! user clicks Allow (docs/re/protocol-handlers.md). Pure JSON edits.

use serde_json::{json, Map, Value};

use crate::settings::Settings;

const SNOOZE_KEY_PATH: [&str; 3] = ["extensions", "ui", "dev_mode_warning_snooze_end_time"];
// A hundred years, in microseconds - "never ask again" in practice.
const SNOOZE_MICROSECONDS: u64 = 100 * 365 * 24 * 60 * 60 * 1_000_000;

const HANDLERS_KEY: &str = "custom_handlers";
const HANDLERS_ENABLED: &str = "enabled";
const REGISTERED: &str = "registered_protocol_handlers";
// Edge's "Block": a handler listed here is never used, and the site's later
// requests to register it are dropped without a prompt.
const IGNORED: &str = "ignored_protocol_handlers";
const PROTOCOL: &str = "protocol";
const URL: &str = "url";
const IS_DEFAULT: &str = "default";

/// A web app that opens one kind of link, as Chrome lets a site register itself.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WebHandler {
    /// Email links (`mailto:`) open a Gmail draft.
    Gmail,
    /// Calendar subscription links (`webcal:`) open in Google Calendar.
    GoogleCalendar,
}

impl WebHandler {
    pub const ALL: [Self; 2] = [Self::Gmail, Self::GoogleCalendar];

    #[must_use]
    pub fn protocol(self) -> &'static str {
        match self {
            Self::Gmail => "mailto",
            Self::GoogleCalendar => "webcal",
        }
    }

    /// The handler URL; Edge puts the escaped link in place of `%s`. Exactly what
    /// each app registers itself with, so an old Block of it is recognized.
    #[must_use]
    pub fn url(self) -> &'static str {
        match self {
            Self::Gmail => "https://mail.google.com/mail/?extsrc=mailto&url=%s",
            Self::GoogleCalendar => "https://calendar.google.com/calendar/u/0/r?cid=%s",
        }
    }
}

/// Every edit the settings ask for, applied to one profile's `Preferences`.
pub fn apply(settings: &Settings, preferences: &mut Value, now_microseconds: u64) {
    if settings.features.quiet_dev_mode {
        snooze_dev_mode_prompt(preferences, now_microseconds);
    }
    set_web_handlers(
        preferences,
        settings.features.web_handlers,
        now_microseconds,
    );
}

/// Push Edge's "turn off developer-mode extensions" prompt a century out.
pub fn snooze_dev_mode_prompt(preferences: &mut Value, now_microseconds: u64) {
    let snooze = (now_microseconds + SNOOZE_MICROSECONDS).to_string();
    let (leaf, parents) = SNOOZE_KEY_PATH
        .split_last()
        .unwrap_or_else(|| unreachable!("the key path is not empty"));
    let mut node = preferences;
    for key in parents {
        node = object_at(node, key);
    }
    if let Some(object) = node.as_object_mut() {
        object.insert((*leaf).to_owned(), Value::String(snooze));
    }
}

/// Register every [`WebHandler`] as its protocol's default (lifting an old
/// Block of it), or, when `is_on` is false, remove the entries this wrote.
/// Handlers the user registered from other sites are kept; they only stop
/// being the default while one of ours is on.
pub fn set_web_handlers(preferences: &mut Value, is_on: bool, now_microseconds: u64) {
    let handlers = object_at(preferences, HANDLERS_KEY);
    for handler in WebHandler::ALL {
        remove_handler(list_at(handlers, IGNORED), handler);
        let registered = list_at(handlers, REGISTERED);
        remove_handler(registered, handler);
        if is_on {
            for entry in registered.iter_mut() {
                let is_same_protocol =
                    entry.get(PROTOCOL).and_then(Value::as_str) == Some(handler.protocol());
                if is_same_protocol {
                    entry[IS_DEFAULT] = Value::Bool(false);
                }
            }
            registered.push(registered_entry(handler, now_microseconds));
        }
    }
    if is_on {
        handlers[HANDLERS_ENABLED] = Value::Bool(true);
    }
}

// The entry Edge itself writes when the user allows a site's handler.
fn registered_entry(handler: WebHandler, now_microseconds: u64) -> Value {
    json!({
        "default": true,
        "is_allowed_in_incognito": false,
        "is_confirmed": true,
        "last_modified": now_microseconds.to_string(),
        "protocol": handler.protocol(),
        "security_level": 0,
        "url": handler.url(),
    })
}

fn remove_handler(list: &mut Vec<Value>, handler: WebHandler) {
    list.retain(|entry| {
        let protocol = entry.get(PROTOCOL).and_then(Value::as_str);
        let url = entry.get(URL).and_then(Value::as_str);
        let is_this_handler = protocol == Some(handler.protocol()) && url == Some(handler.url());
        !is_this_handler
    });
}

// The object under `key`, created (or replacing a non-object) as needed.
fn object_at<'a>(node: &'a mut Value, key: &str) -> &'a mut Value {
    if !node.is_object() {
        *node = Value::Object(Map::new());
    }
    let child = node
        .as_object_mut()
        .unwrap_or_else(|| unreachable!("made an object above"))
        .entry(key.to_owned())
        .or_insert_with(|| Value::Object(Map::new()));
    if !child.is_object() {
        *child = Value::Object(Map::new());
    }
    child
}

// The list under `key` of an object, created (or replacing a non-list) as needed.
fn list_at<'a>(object: &'a mut Value, key: &str) -> &'a mut Vec<Value> {
    let child = &mut object[key];
    if !child.is_array() {
        *child = Value::Array(Vec::new());
    }
    child
        .as_array_mut()
        .unwrap_or_else(|| unreachable!("made a list above"))
}

#[cfg(test)]
mod tests {
    use super::*;

    const NOW: u64 = 13_436_736_371_278_250;

    fn list(preferences: &Value, key: &str) -> Vec<Value> {
        preferences[HANDLERS_KEY][key]
            .as_array()
            .cloned()
            .unwrap_or_default()
    }

    fn registered(preferences: &Value) -> Vec<Value> {
        list(preferences, REGISTERED)
    }

    #[test]
    fn turning_on_registers_both_as_default() {
        let mut preferences = json!({});
        set_web_handlers(&mut preferences, true, NOW);
        let entries = registered(&preferences);
        assert_eq!(entries.len(), 2);
        assert_eq!(entries[0][PROTOCOL], "mailto");
        assert_eq!(entries[0][URL], WebHandler::Gmail.url());
        assert_eq!(entries[0][IS_DEFAULT], true);
        assert_eq!(entries[0]["last_modified"], NOW.to_string());
        assert_eq!(preferences[HANDLERS_KEY][HANDLERS_ENABLED], true);
    }

    #[test]
    fn an_old_block_is_lifted() {
        let mut preferences = json!({ "custom_handlers": { "ignored_protocol_handlers": [
            { "protocol": "mailto", "url": WebHandler::Gmail.url(), "last_modified": "1" },
            { "protocol": "irc", "url": "https://chat.example/?%s", "last_modified": "1" },
        ]}});
        set_web_handlers(&mut preferences, true, NOW);
        let ignored = list(&preferences, IGNORED);
        assert_eq!(ignored.len(), 1);
        assert_eq!(ignored[0][PROTOCOL], "irc");
    }

    #[test]
    fn another_sites_handler_is_kept_but_not_default() {
        let mut preferences = json!({ "custom_handlers": { "registered_protocol_handlers": [
            { "protocol": "mailto", "url": "https://outlook.example/?%s", "default": true },
        ]}});
        set_web_handlers(&mut preferences, true, NOW);
        let entries = registered(&preferences);
        assert_eq!(entries.len(), 3);
        assert_eq!(entries[0][IS_DEFAULT], false);
    }

    #[test]
    fn turning_on_twice_does_not_duplicate() {
        let mut preferences = json!({});
        set_web_handlers(&mut preferences, true, NOW);
        set_web_handlers(&mut preferences, true, NOW + 1);
        assert_eq!(registered(&preferences).len(), 2);
    }

    #[test]
    fn turning_off_removes_only_ours() {
        let mut preferences = json!({ "custom_handlers": { "registered_protocol_handlers": [
            { "protocol": "mailto", "url": "https://outlook.example/?%s", "default": false },
        ]}});
        set_web_handlers(&mut preferences, true, NOW);
        set_web_handlers(&mut preferences, false, NOW);
        let entries = registered(&preferences);
        assert_eq!(entries.len(), 1);
        assert_eq!(entries[0][URL], "https://outlook.example/?%s");
    }

    #[test]
    fn the_snooze_is_a_century_out() {
        let mut preferences = json!({ "extensions": { "ui": 5 } });
        snooze_dev_mode_prompt(&mut preferences, NOW);
        assert_eq!(
            preferences["extensions"]["ui"]["dev_mode_warning_snooze_end_time"],
            (NOW + SNOOZE_MICROSECONDS).to_string()
        );
    }
}
