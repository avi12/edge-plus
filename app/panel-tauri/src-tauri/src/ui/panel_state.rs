//! Everything the window shows, read fresh after every action.

use crate::edge::version::edge_support::{self, FeatureState};
use crate::edge::version::file_version::FileVersion;
use crate::platform::settings::{Feature, Features, Theming};
use crate::themes::sources::installed_themes::InstalledTheme;

#[derive(Clone, Debug)]
#[allow(clippy::struct_excessive_bools, reason = "each is an independent fact")]
pub struct PanelState {
    pub features: Features,
    pub theming: Theming,
    /// Themes installed in Edge that the panel did not add.
    pub edge_themes: Vec<InstalledTheme>,
    pub is_edge_running: bool,
    pub is_start_with_windows: bool,
    pub is_split_loaded: bool,
    /// The switches on want the DLL in Edge (`dll_sync::is_wanted`).
    pub is_split_wanted: bool,
    pub is_auto_inject: bool,
    pub is_auto_update: bool,
    /// The running build, else the installed one; None when it cannot be read.
    pub edge_build: Option<FileVersion>,
    pub supported_edge_build: FileVersion,
}

impl PanelState {
    #[must_use]
    pub fn state_of(&self, feature: Feature) -> FeatureState {
        edge_support::state_of(
            &self.features,
            feature,
            self.edge_build,
            self.supported_edge_build,
        )
    }

    /// A paused or unavailable switch is held as it is until a supported build.
    #[must_use]
    pub fn is_locked(&self, feature: Feature) -> bool {
        matches!(
            self.state_of(feature),
            FeatureState::Paused | FeatureState::Unavailable
        )
    }

    #[must_use]
    pub fn paused(&self) -> Vec<Feature> {
        edge_support::paused(&self.features, self.edge_build, self.supported_edge_build)
    }
}
