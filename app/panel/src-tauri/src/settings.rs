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

/// The browser assistant the toolbar chat button opens. The wire values match
/// `config::Assistant` in `app/native-split/src/config.h`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Assistant {
    /// Edge's own Copilot: the button and pane stay stock.
    #[default]
    Copilot,
    /// Gemini: the button is reskinned and opens Gemini in a side panel.
    Gemini,
}

impl Assistant {
    /// The u32 the DLL reads from the config file (see `config.h`).
    #[must_use]
    pub fn wire(self) -> u32 {
        match self {
            Self::Copilot => 0,
            Self::Gemini => 1,
        }
    }
}

/// The feature switches, with their defaults (split on, the rest off).
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
#[allow(
    clippy::struct_excessive_bools,
    reason = "each is an independent on/off switch"
)]
pub struct Features {
    pub native_split: bool,
    pub theming: bool,
    pub quiet_dev_mode: bool,
    /// A site's request to handle a link type is only blocked when the user
    /// says so (the DLL's `protocol_handlers.h`).
    pub protocol_handlers: bool,
    /// Which assistant the toolbar chat button opens. Not a switch but a
    /// choice; picking Gemini is what [`Feature::Assistant`] treats as "on"
    /// (it calls into `msedge.dll`, so an Edge update forces it back to
    /// Copilot the same way a mismatched build switches the others off).
    pub assistant: Assistant,
    /// Edge's own Ctrl+F bar also takes `/pattern/flags` (a `RegExp` search)
    /// and `? question` (ask the chosen [`Assistant`]); the DLL's `find.h`.
    pub enhanced_find: bool,
}

impl Default for Features {
    fn default() -> Self {
        Self {
            native_split: true,
            theming: false,
            quiet_dev_mode: false,
            protocol_handlers: false,
            assistant: Assistant::Copilot,
            enhanced_find: false,
        }
    }
}

/// One feature the panel owns, named the way [`Features`] serializes it.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum Feature {
    NativeSplit,
    Theming,
    QuietDevMode,
    ProtocolHandlers,
    /// The non-default assistant (Gemini) is selected.
    Assistant,
    EnhancedFind,
}

impl Feature {
    pub const ALL: [Self; 6] = [
        Self::NativeSplit,
        Self::Theming,
        Self::QuietDevMode,
        Self::ProtocolHandlers,
        Self::Assistant,
        Self::EnhancedFind,
    ];
}

impl Features {
    #[must_use]
    pub fn is_on(self, feature: Feature) -> bool {
        match feature {
            Feature::NativeSplit => self.native_split,
            Feature::Theming => self.theming,
            Feature::QuietDevMode => self.quiet_dev_mode,
            Feature::ProtocolHandlers => self.protocol_handlers,
            Feature::Assistant => self.assistant == Assistant::Gemini,
            Feature::EnhancedFind => self.enhanced_find,
        }
    }

    /// Whether any switch that lives in the injected DLL is on, so the DLL
    /// must be loaded.
    #[must_use]
    pub fn needs_dll(self) -> bool {
        self.native_split
            || self.theming
            || self.protocol_handlers
            || self.assistant == Assistant::Gemini
            || self.enhanced_find
    }

    pub fn set(&mut self, feature: Feature, is_on: bool) {
        match feature {
            Feature::NativeSplit => self.native_split = is_on,
            Feature::Theming => self.theming = is_on,
            Feature::QuietDevMode => self.quiet_dev_mode = is_on,
            Feature::ProtocolHandlers => self.protocol_handlers = is_on,
            Feature::Assistant => {
                self.assistant = if is_on {
                    Assistant::Gemini
                } else {
                    Assistant::Copilot
                };
            }
            Feature::EnhancedFind => self.enhanced_find = is_on,
        }
    }
}

/// The whole settings file.
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub features: Features,
    pub theming: Theming,
    /// The `msedge.dll` build last seen running, so a different one is
    /// recognized as an Edge update.
    #[serde(rename = "lastEdgeBuild")]
    pub last_edge_build: Option<String>,
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
