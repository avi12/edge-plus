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
    /// `tab_shape.h`
    TabShape,
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
        Self::TabShape,
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
    /// own. Saved under its old name so a saved choice carries over.
    #[serde(rename = "tabSpine")]
    pub tab_shape: bool,
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
            tab_shape: true,
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
            Feature::TabShape => self.tab_shape,
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
            Feature::TabShape => &mut self.tab_shape,
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
}

impl Theming {
    pub fn assign(&mut self, mode: ColorMode, id: &str) {
        let slot = match mode {
            ColorMode::Light => &mut self.light,
            ColorMode::Dark => &mut self.dark,
        };
        id.clone_into(slot);
    }

    pub fn add_or_replace(&mut self, entry: ThemeEntry) {
        match self.themes.iter_mut().find(|theme| theme.id == entry.id) {
            Some(existing) => *existing = entry,
            None => self.themes.push(entry),
        }
    }

    pub fn reorder(&mut self, ids: &[String]) {
        self.themes = theme_order::ordered(&self.themes, ids);
    }

    pub fn remove(&mut self, id: &str) {
        self.themes.retain(|theme| theme.id != id);
        if self.light == id {
            self.light.clear();
        }
        if self.dark == id {
            self.dark.clear();
        }
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

    /// Garbage is the defaults.
    #[must_use]
    pub fn parse(text: &str) -> Self {
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
    fn defaults_are_split_and_tab_shape_on_the_rest_off() {
        let settings = Settings::default();
        for feature in Feature::ALL {
            let is_expected_on = matches!(feature, Feature::NativeSplit | Feature::TabShape);
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
        theming.remove("a");
        assert!(theming.light.is_empty() && theming.dark.is_empty());
        assert_eq!(Vec::<ThemeEntry>::new(), theming.themes);
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
