//! The user's persisted choices: which switches are on, and the theme library
//! with the light/dark assignment. Stored as `settings.json` in the data
//! folder; only this file is the panel's own, every other file is a contract
//! with Edge or the DLL.

use serde::{Deserialize, Serialize};

use crate::paths;

/// Where a theme came from, so the installer knows how to add it to Edge.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum ThemeSource {
    Store,
    Local,
}

/// One theme the user added: an installed Edge theme extension by id.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ThemeEntry {
    pub id: String,
    pub name: String,
    pub source: ThemeSource,
    /// The store URL or local `.crx` path it was added from.
    pub origin: String,
}

/// The theme library and which theme to use in each Windows mode (an empty id
/// means the default theme).
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct Theming {
    pub themes: Vec<ThemeEntry>,
    pub light: String,
    pub dark: String,
}

/// The three feature switches, with their defaults (split on, the rest off).
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub struct Features {
    pub native_split: bool,
    pub theming: bool,
    pub quiet_dev_mode: bool,
}

impl Default for Features {
    fn default() -> Self {
        Self {
            native_split: true,
            theming: false,
            quiet_dev_mode: false,
        }
    }
}

/// The whole settings file.
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub features: Features,
    pub theming: Theming,
}

impl Settings {
    /// Read the settings file, falling back to defaults when it is missing or
    /// unreadable.
    #[must_use]
    pub fn load() -> Self {
        std::fs::read_to_string(paths::settings())
            .ok()
            .and_then(|text| serde_json::from_str(&text).ok())
            .unwrap_or_default()
    }

    /// Write the settings file atomically (whole, then swapped in).
    ///
    /// # Errors
    /// Returns any filesystem or serialization error.
    pub fn save(&self) -> std::io::Result<()> {
        let path = paths::settings();
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        let serialized = serde_json::to_string_pretty(self)?;
        let temporary = path.with_extension("json.tmp");
        std::fs::write(&temporary, serialized)?;
        std::fs::rename(&temporary, &path)
    }
}

/// The two Windows appearance modes a theme can be assigned to.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    Light,
    Dark,
}
