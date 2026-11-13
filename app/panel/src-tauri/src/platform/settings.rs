//! The user's choices, in `settings.json`. The file keeps exactly the shape the
//! earlier panels (Tauri, then `WinUI`) wrote, so an existing file carries over;
//! keep it so.

use std::fs;
use std::io;
use std::path::Path;
use std::sync::{Mutex, MutexGuard};

use serde::{Deserialize, Deserializer, Serialize};

use crate::platform::paths;
use crate::themes::theme_order;

const BYTE_ORDER_MARK: char = '\u{feff}';

/// Held by every load-change-save-apply sequence, so the window's actions and
/// the background watchers never interleave on the file or on the DLL.
static GATE: Mutex<()> = Mutex::new(());

/// Takes the gate; a panic that poisoned it left nothing half-written that the
/// next load would not read past.
pub fn gate() -> MutexGuard<'static, ()> {
    GATE.lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(ts_rs::TS))]
pub enum Feature {
    NativeSplit,
    Theming,
    QuietDevMode,
    /// `protocol_handlers.h`
    ProtocolHandlers,
    /// find.h
    EnhancedFind,
    /// `theme_follow.h`
    WebUiFollowsTheme,
    /// `live_preview.h`
    LivePreview,
    /// `cyberpunk_skin.h` and `tab_shape.h`
    CyberpunkSkin,
    /// `reading_mode.h`; the two themes are picked in reading mode itself and
    /// kept by the DLL.
    ReadingTheme,
}

impl Feature {
    pub const ALL: [Self; 9] = [
        Self::NativeSplit,
        Self::Theming,
        Self::QuietDevMode,
        Self::ProtocolHandlers,
        Self::EnhancedFind,
        Self::WebUiFollowsTheme,
        Self::LivePreview,
        Self::CyberpunkSkin,
        Self::ReadingTheme,
    ];
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum ThemeSource {
    #[default]
    Store,
    Local,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
#[cfg_attr(test, derive(ts_rs::TS))]
pub enum ColorMode {
    Light,
    Dark,
}

/// A settings file written when a removed switch existed still loads: an
/// unknown field is ignored.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
#[allow(
    clippy::struct_excessive_bools,
    reason = "each is an independent switch"
)]
pub struct Features {
    pub native_split: bool,
    pub theming: bool,
    pub quiet_dev_mode: bool,
    pub protocol_handlers: bool,
    pub enhanced_find: bool,
    pub web_ui_follows_theme: bool,
    pub live_preview: bool,
    /// On by default: it was part of the theming until it got a switch of its
    /// own. Saved under its oldest name (the cut-corner tabs' switch, which the
    /// skin replaced) so a saved choice carries over.
    #[serde(rename = "tabSpine")]
    pub cyberpunk_skin: bool,
    pub reading_theme: bool,
}

impl Default for Features {
    fn default() -> Self {
        Self {
            native_split: true,
            theming: false,
            quiet_dev_mode: false,
            protocol_handlers: false,
            enhanced_find: false,
            web_ui_follows_theme: false,
            live_preview: false,
            cyberpunk_skin: true,
            reading_theme: false,
        }
    }
}

impl Features {
    /// Every switch off: the starting point for the effective set.
    #[must_use]
    pub fn none() -> Self {
        let mut features = Self::default();
        for feature in Feature::ALL {
            features.set(feature, false);
        }
        features
    }

    #[must_use]
    pub fn is_on(&self, feature: Feature) -> bool {
        match feature {
            Feature::NativeSplit => self.native_split,
            Feature::Theming => self.theming,
            Feature::QuietDevMode => self.quiet_dev_mode,
            Feature::ProtocolHandlers => self.protocol_handlers,
            Feature::EnhancedFind => self.enhanced_find,
            Feature::WebUiFollowsTheme => self.web_ui_follows_theme,
            Feature::LivePreview => self.live_preview,
            Feature::CyberpunkSkin => self.cyberpunk_skin,
            Feature::ReadingTheme => self.reading_theme,
        }
    }

    pub fn set(&mut self, feature: Feature, is_on: bool) {
        let switch = match feature {
            Feature::NativeSplit => &mut self.native_split,
            Feature::Theming => &mut self.theming,
            Feature::QuietDevMode => &mut self.quiet_dev_mode,
            Feature::ProtocolHandlers => &mut self.protocol_handlers,
            Feature::EnhancedFind => &mut self.enhanced_find,
            Feature::WebUiFollowsTheme => &mut self.web_ui_follows_theme,
            Feature::LivePreview => &mut self.live_preview,
            Feature::CyberpunkSkin => &mut self.cyberpunk_skin,
            Feature::ReadingTheme => &mut self.reading_theme,
        };
        *switch = is_on;
    }

    /// `QuietDevMode` is the panel's own, not the DLL's.
    #[must_use]
    pub fn needs_dll(&self) -> bool {
        Feature::ALL
            .into_iter()
            .any(|feature| feature != Feature::QuietDevMode && self.is_on(feature))
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct ThemeEntry {
    #[serde(default)]
    pub id: String,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub source: ThemeSource,
    /// The store URL or local path it was added from.
    #[serde(default)]
    pub origin: String,
}

/// An empty light / dark id means Edge's default theme.
#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default)]
pub struct Theming {
    #[serde(deserialize_with = "null_as_default")]
    pub themes: Vec<ThemeEntry>,
    #[serde(deserialize_with = "null_as_default")]
    pub light: String,
    #[serde(deserialize_with = "null_as_default")]
    pub dark: String,
    /// The ids of all themes, own and Edge's, in display order (see
    /// `theme_order::combined`); empty means the default order.
    #[serde(deserialize_with = "null_as_default")]
    pub order: Vec<String>,
    /// The themes each slot wore before its current one, the latest last: what
    /// a slot goes back to when its theme is removed. Edge's default theme is
    /// one of them, as an empty id.
    #[serde(deserialize_with = "null_as_default")]
    pub light_history: Vec<String>,
    #[serde(deserialize_with = "null_as_default")]
    pub dark_history: Vec<String>,
}

/// How many earlier themes a slot remembers.
const HISTORY_LIMIT: usize = 8;

impl Theming {
    /// The slot of `mode` wears `id`, and the one it wore before is remembered.
    pub fn assign(&mut self, mode: ColorMode, id: &str) {
        let previous = self.slot(mode).to_owned();
        let is_change = previous != id;
        let history = self.history_mut(mode);
        history.retain(|earlier| earlier != id);
        if is_change {
            history.retain(|earlier| earlier != &previous);
            history.push(previous);
            let excess = history.len().saturating_sub(HISTORY_LIMIT);
            history.drain(..excess);
        }
        id.clone_into(self.slot_mut(mode));
    }

    fn slot_mut(&mut self, mode: ColorMode) -> &mut String {
        match mode {
            ColorMode::Light => &mut self.light,
            ColorMode::Dark => &mut self.dark,
        }
    }

    fn history_mut(&mut self, mode: ColorMode) -> &mut Vec<String> {
        match mode {
            ColorMode::Light => &mut self.light_history,
            ColorMode::Dark => &mut self.dark_history,
        }
    }

    /// The theme id the slot of `mode` wears; empty is Edge's default theme.
    #[must_use]
    pub fn slot(&self, mode: ColorMode) -> &str {
        match mode {
            ColorMode::Light => &self.light,
            ColorMode::Dark => &self.dark,
        }
    }

    /// The slot goes back to Edge's default theme; every theme stays listed.
    pub fn clear_slot(&mut self, mode: ColorMode) {
        self.assign(mode, "");
    }

    pub fn add_or_replace(&mut self, entry: ThemeEntry) {
        match self.themes.iter_mut().find(|theme| theme.id == entry.id) {
            Some(existing) => *existing = entry,
            None => self.themes.push(entry),
        }
    }

    /// `entry` takes the place of the theme `old_id`: its position, its slots,
    /// its order and its history carry over, so a theme that moved folders is
    /// the same theme to the panel.
    pub fn relink(&mut self, old_id: &str, entry: ThemeEntry) {
        let new_id = entry.id.clone();
        let rename = |id: &mut String| {
            if id == old_id {
                id.clone_from(&new_id);
            }
        };
        self.themes.retain(|theme| theme.id != new_id);
        match self.themes.iter_mut().find(|theme| theme.id == old_id) {
            Some(existing) => *existing = entry,
            None => self.themes.push(entry),
        }
        rename(&mut self.light);
        rename(&mut self.dark);
        self.order.iter_mut().for_each(rename);
        self.light_history.iter_mut().for_each(rename);
        self.dark_history.iter_mut().for_each(rename);
    }

    pub fn reorder(&mut self, ids: &[String]) {
        self.themes = theme_order::ordered(&self.themes, ids);
        self.order = ids.to_vec();
    }

    /// Whether the light or the dark slot wears the theme.
    pub fn is_assigned(&self, id: &str) -> bool {
        self.light == id || self.dark == id
    }

    /// A slot that wore the theme goes back to the theme it wore before, when
    /// that is still `is_available`, and else to Edge's default theme. The
    /// theme, and any earlier one no longer available, is forgotten from
    /// every slot's history.
    pub fn unassign(&mut self, id: &str, is_available: impl Fn(&str) -> bool) {
        for mode in [ColorMode::Light, ColorMode::Dark] {
            let history = self.history_mut(mode);
            history
                .retain(|earlier| earlier != id && (earlier.is_empty() || is_available(earlier)));
            let previous = history.last().cloned();
            if self.slot(mode) != id {
                continue;
            }
            let back_to = previous.unwrap_or_default();
            self.history_mut(mode).retain(|earlier| earlier != &back_to);
            back_to.clone_into(self.slot_mut(mode));
        }
    }

    /// A theme only Edge knows is gone: its slots and its place in the order.
    pub fn forget_edge_theme(&mut self, id: &str, is_available: impl Fn(&str) -> bool) {
        self.unassign(id, is_available);
        self.order.retain(|ordered| ordered != id);
    }

    pub fn remove(&mut self, id: &str, is_available: impl Fn(&str) -> bool) {
        self.themes.retain(|theme| theme.id != id);
        self.forget_edge_theme(id, is_available);
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub struct Settings {
    #[serde(deserialize_with = "null_as_default")]
    pub features: Features,
    #[serde(deserialize_with = "null_as_default")]
    pub theming: Theming,
    /// A different build than this one running is recognized as an Edge update.
    pub last_edge_build: Option<String>,
    /// Puts the DLL into an Edge that (re)starts while the panel runs; off, the
    /// panel offers an Inject button instead. On by default, as it always was.
    pub auto_inject: bool,
    /// Looks for a newer Edge+ release (`update_watch`) and offers to restart
    /// into it. On by default; off, Edge+ never contacts GitHub.
    pub auto_update: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            features: Features::default(),
            theming: Theming::default(),
            last_edge_build: None,
            auto_inject: true,
            auto_update: true,
        }
    }
}

/// An explicit null in the file is a missing section, not a failure later.
fn null_as_default<'de, D, T>(deserializer: D) -> Result<T, D::Error>
where
    D: Deserializer<'de>,
    T: Default + Deserialize<'de>,
{
    Ok(Option::<T>::deserialize(deserializer)?.unwrap_or_default())
}

impl Settings {
    #[must_use]
    pub fn load() -> Self {
        Self::load_from(&paths::settings())
    }

    /// A missing or unreadable file is the defaults.
    #[must_use]
    pub fn load_from(path: &Path) -> Self {
        fs::read_to_string(path).map_or_else(|_| Self::default(), |text| Self::parse(&text))
    }

    /// Garbage is the defaults. A leading byte-order mark is skipped, as the
    /// C# panel's File.ReadAllText did: a file saved by a Windows editor or
    /// PowerShell 5 starts with one.
    #[must_use]
    pub fn parse(text: &str) -> Self {
        let text = text.strip_prefix(BYTE_ORDER_MARK).unwrap_or(text);
        serde_json::from_str(text).unwrap_or_default()
    }

    #[must_use]
    pub fn to_json(&self) -> String {
        serde_json::to_string_pretty(self).unwrap_or_default()
    }

    pub fn save(&self) -> io::Result<()> {
        self.save_to(&paths::settings())
    }

    pub fn save_to(&self, path: &Path) -> io::Result<()> {
        paths::write_atomically(path, self.to_json().as_bytes())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_relinked_theme_keeps_its_place_and_slots() {
        let entry = |id: &str, origin: &str| ThemeEntry {
            id: id.to_owned(),
            name: "Dusk".to_owned(),
            source: ThemeSource::Local,
            origin: origin.to_owned(),
        };
        let mut theming = Theming {
            themes: vec![entry("old", r"C:\old"), entry("other", r"C:\other")],
            light: "old".to_owned(),
            order: vec!["other".to_owned(), "old".to_owned()],
            ..Theming::default()
        };
        theming.relink("old", entry("new", r"C:\new"));
        assert_eq!("new", theming.themes[0].id, "same position");
        assert_eq!(r"C:\new", theming.themes[0].origin);
        assert_eq!("new", theming.light, "the slot follows");
        assert_eq!(vec!["other", "new"], theming.order, "the order follows");
    }

    #[test]
    fn a_byte_order_mark_is_read_past() {
        assert!(!Settings::parse("\u{feff}{\"autoInject\":false}").auto_inject);
    }

    // A file from before the switch existed checks for updates.
    #[test]
    fn auto_update_defaults_on_and_is_saved_camel_case() {
        assert!(Settings::default().auto_update);
        assert!(Settings::parse(r#"{"autoInject":true}"#).auto_update);
        assert!(!Settings::parse(r#"{"autoUpdate":false}"#).auto_update);
        let json = Settings {
            auto_update: false,
            ..Settings::default()
        }
        .to_json();
        assert!(json.contains("\"autoUpdate\": false"));
        assert!(!Settings::parse(&json).auto_update);
    }

    // A file from before the switch existed keeps the old behaviour: inject.
    #[test]
    fn auto_inject_defaults_on_and_is_saved_camel_case() {
        assert!(Settings::default().auto_inject);
        assert!(Settings::parse(r#"{"features":{"nativeSplit":true}}"#).auto_inject);
        assert!(!Settings::parse(r#"{"autoInject":false}"#).auto_inject);
        let json = Settings {
            auto_inject: false,
            ..Settings::default()
        }
        .to_json();
        assert!(json.contains("\"autoInject\": false"));
        assert!(!Settings::parse(&json).auto_inject);
    }

    // The reading themes are picked in reading mode itself, so the old
    // "readingMode" object is read past and never written.
    #[test]
    fn reading_mode_is_only_its_switch() {
        assert!(!Settings::default().features.reading_theme);
        let loaded = Settings::parse(
            r#"{"features":{"readingTheme":true},"readingMode":{"light":"skyBlue","dark":"darkGrey"}}"#,
        );
        assert!(loaded.features.reading_theme);
        let json = loaded.to_json();
        assert!(json.contains("\"readingTheme\": true"));
        assert!(!json.contains("readingMode"));
    }

    #[test]
    fn defaults_are_split_and_cyberpunk_skin_on_the_rest_off() {
        let settings = Settings::default();
        for feature in Feature::ALL {
            let is_expected_on = matches!(feature, Feature::NativeSplit | Feature::CyberpunkSkin);
            assert_eq!(
                is_expected_on,
                settings.features.is_on(feature),
                "{feature:?}"
            );
        }
        assert!(settings.features.needs_dll());
    }

    #[test]
    fn a_file_from_the_gemini_days_still_loads() {
        let settings = Settings::parse(
            r#"{"features":{"nativeSplit":false,"assistant":"gemini","enhancedFind":true}}"#,
        );
        assert!(!settings.features.native_split);
        assert!(settings.features.enhanced_find);
        assert_eq!("", settings.theming.light);
    }

    // The shape the earlier panels wrote.
    #[test]
    fn an_existing_file_loads() {
        let text = r#"
        {
          "features": { "nativeSplit": true, "theming": true, "quietDevMode": false,
                        "protocolHandlers": true, "enhancedFind": false,
                        "webUiFollowsTheme": true, "livePreview": false },
          "theming": {
            "themes": [ { "id": "abc", "name": "Dusk", "source": "store", "origin": "https://x" },
                        { "id": "def", "name": "Mine", "source": "local", "origin": "C:\\t" } ],
            "light": "abc", "dark": "def"
          },
          "lastEdgeBuild": "155.0.4283.62"
        }"#;
        let settings = Settings::parse(text);
        assert!(settings.features.theming && settings.features.protocol_handlers);
        assert_eq!(2, settings.theming.themes.len());
        assert_eq!(ThemeSource::Store, settings.theming.themes[0].source);
        assert_eq!(ThemeSource::Local, settings.theming.themes[1].source);
        assert_eq!(r"C:\t", settings.theming.themes[1].origin);
        assert_eq!("def", settings.theming.dark);
        assert_eq!(Some("155.0.4283.62"), settings.last_edge_build.as_deref());
    }

    #[test]
    fn writes_the_existing_shape() {
        let mut settings = Settings::default();
        settings.theming.add_or_replace(ThemeEntry {
            id: "abc".into(),
            name: "Dusk".into(),
            source: ThemeSource::Local,
            origin: "o".into(),
        });
        let json = settings.to_json();
        for key in [
            "\"features\"",
            "\"nativeSplit\"",
            "\"webUiFollowsTheme\"",
            "\"livePreview\"",
            "\"tabSpine\"",
            "\"theming\"",
            "\"themes\"",
            "\"source\": \"local\"",
            "\"lastEdgeBuild\"",
        ] {
            assert!(json.contains(key), "the file has {key}");
        }
        assert!(!json.to_lowercase().contains("needsdll"));
    }

    #[test]
    fn round_trips_through_a_file() {
        let path =
            std::env::temp_dir().join(format!("edgeplus-settings-{}.json", std::process::id()));
        let mut settings = Settings::default();
        settings.features.set(Feature::LivePreview, true);
        settings.theming.assign(ColorMode::Light, "abc");
        settings.save_to(&path).expect("save");
        let loaded = Settings::load_from(&path);
        fs::remove_file(&path).expect("remove");
        assert!(loaded.features.live_preview);
        assert_eq!("abc", loaded.theming.light);
        assert!(!path.with_extension("json.tmp").exists());
        assert!(Settings::load_from(&path).features.native_split);
    }

    #[test]
    fn broken_or_null_sections_fall_back() {
        assert!(Settings::parse("not json").features.native_split);
        let nulls = Settings::parse(r#"{"features":null,"theming":null}"#);
        assert!(nulls.features.native_split);
        assert_eq!(Vec::<ThemeEntry>::new(), nulls.theming.themes);
    }

    #[test]
    fn removing_a_theme_clears_its_assignments() {
        let mut theming = Theming::default();
        let entry = |name: &str| ThemeEntry {
            id: "a".into(),
            name: name.into(),
            source: ThemeSource::Local,
            origin: String::new(),
        };
        theming.add_or_replace(entry("A"));
        theming.add_or_replace(entry("A2"));
        assert_eq!(1, theming.themes.len());
        assert_eq!("A2", theming.themes[0].name);
        theming.assign(ColorMode::Light, "a");
        theming.assign(ColorMode::Dark, "a");
        theming.remove("a", |_| false);
        assert!(theming.light.is_empty() && theming.dark.is_empty());
        assert_eq!(Vec::<ThemeEntry>::new(), theming.themes);
    }

    #[test]
    fn clearing_a_slot_keeps_every_theme_and_the_other_slot() {
        let mut theming = Theming::default();
        theming.add_or_replace(ThemeEntry {
            id: "a".into(),
            name: "A".into(),
            source: ThemeSource::Local,
            origin: String::new(),
        });
        theming.assign(ColorMode::Light, "a");
        theming.assign(ColorMode::Dark, "a");
        theming.clear_slot(ColorMode::Light);
        assert_eq!("", theming.slot(ColorMode::Light));
        assert_eq!("a", theming.slot(ColorMode::Dark));
        assert_eq!(1, theming.themes.len());
    }

    #[test]
    fn unassigning_a_theme_keeps_it_and_frees_only_its_slots() {
        let mut theming = Theming::default();
        theming.add_or_replace(ThemeEntry {
            id: "a".into(),
            name: "A".into(),
            source: ThemeSource::Local,
            origin: String::new(),
        });
        theming.assign(ColorMode::Light, "a");
        theming.assign(ColorMode::Dark, "b");
        assert!(theming.is_assigned("a") && theming.is_assigned("b"));
        theming.unassign("a", |_| false);
        assert!(
            theming.light.is_empty(),
            "the light slot is the default again"
        );
        assert_eq!("b", theming.dark, "another theme's slot stays");
        assert!(!theming.is_assigned("a"));
        assert_eq!(1, theming.themes.len(), "the theme stays in the list");
    }

    #[test]
    fn a_removed_theme_gives_its_slot_back_to_the_one_before() {
        let mut theming = Theming::default();
        theming.assign(ColorMode::Dark, "first");
        theming.assign(ColorMode::Dark, "second");
        theming.assign(ColorMode::Dark, "third");
        theming.unassign("third", |id| id != "second");
        assert_eq!("first", theming.dark, "the latest one still there");
        theming.unassign("first", |_| true);
        assert_eq!("", theming.dark, "nothing earlier is left: the default");
        assert!(theming.dark_history.is_empty(), "the history is spent");
    }

    #[test]
    fn removing_a_theme_set_from_the_default_goes_back_to_the_default() {
        let mut theming = Theming::default();
        theming.assign(ColorMode::Dark, "old");
        theming.assign(ColorMode::Dark, "");
        theming.assign(ColorMode::Dark, "new");
        theming.unassign("new", |_| true);
        assert_eq!("", theming.dark, "the default, not the theme before it");
    }

    #[test]
    fn a_removed_theme_is_forgotten_by_the_other_slot_too() {
        let mut theming = Theming::default();
        theming.assign(ColorMode::Light, "gone");
        theming.assign(ColorMode::Light, "kept");
        theming.assign(ColorMode::Dark, "gone");
        theming.unassign("gone", |_| true);
        assert_eq!("kept", theming.light, "a slot that did not wear it stays");
        assert_eq!(
            vec![String::new()],
            theming.light_history,
            "and forgets it: only the default before it is left"
        );
        assert_eq!("", theming.dark);
    }

    #[test]
    fn forgetting_an_edge_theme_frees_its_slots_and_its_place_in_the_order() {
        let mut theming = Theming::default();
        theming.assign(ColorMode::Light, "a");
        theming.assign(ColorMode::Dark, "b");
        theming.reorder(&["b".to_owned(), "a".to_owned(), "c".to_owned()]);
        theming.forget_edge_theme("a", |_| false);
        assert!(theming.light.is_empty(), "the slot is the default again");
        assert_eq!("b", theming.dark, "another theme's slot stays");
        assert_eq!(vec!["b".to_owned(), "c".to_owned()], theming.order);
    }

    #[test]
    fn an_old_file_without_an_order_loads_with_the_default_one() {
        let theming: Theming = serde_json::from_str(r#"{"edgeOrder":["x"],"order":null}"#)
            .expect("an old theming section loads");
        assert_eq!(Theming::default(), theming);
    }

    #[test]
    fn the_switches_map_one_to_one() {
        for feature in Feature::ALL {
            let mut features = Features::none();
            features.set(feature, true);
            for other in Feature::ALL {
                assert_eq!(
                    other == feature,
                    features.is_on(other),
                    "{feature:?} reads {other:?}"
                );
            }
            let is_panel_own = feature == Feature::QuietDevMode;
            assert_eq!(
                !is_panel_own,
                features.needs_dll(),
                "{feature:?} needs the DLL"
            );
        }
    }
}
