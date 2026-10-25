//! The pinned build comes from offsets.h itself (via shared), so the panel
//! compares against exactly what the embedded DLL was reversed against. Every
//! DLL feature calls into msedge.dll by RVA, so any other build breaks them all;
//! the developer-mode prompt only edits Preferences and survives an update.

use crate::edge::file_version::FileVersion;
use crate::platform::settings::Feature;
use crate::platform::shared;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BuildChange {
    pub edge_build: FileVersion,
    pub previous_build: Option<FileVersion>,
    pub supported_build: FileVersion,
    pub paused: Vec<Feature>,
}

fn part(value: u32) -> u16 {
    u16::try_from(value).expect("offsets.h's version parts fit 16 bits")
}

#[must_use]
pub fn pinned() -> FileVersion {
    FileVersion::new(
        part(shared::EDGE_VERSION_MAJOR),
        part(shared::EDGE_VERSION_MINOR),
        part(shared::EDGE_VERSION_BUILD),
        part(shared::EDGE_VERSION_REVISION),
    )
}

#[must_use]
pub fn depends_on_pinned_build(feature: Feature) -> bool {
    match feature {
        Feature::NativeSplit
        | Feature::Theming
        | Feature::ProtocolHandlers
        | Feature::EnhancedFind
        | Feature::WebUiFollowsTheme
        | Feature::LivePreview
        | Feature::TabShape
        | Feature::ReadingTheme => true,
        Feature::QuietDevMode => false,
    }
}

/// Any process on a build other than the pinned one decides (the DLL must not
/// go into it); None when none could be read (Edge is not running).
#[must_use]
pub fn judged(builds: &[FileVersion], pinned: FileVersion) -> Option<FileVersion> {
    builds
        .iter()
        .copied()
        .find(|&build| build != pinned)
        .or_else(|| builds.first().copied())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::edge::edge_support;
    use crate::platform::settings::Features;

    const DLL_FEATURES: [Feature; 8] = [
        Feature::NativeSplit,
        Feature::Theming,
        Feature::ProtocolHandlers,
        Feature::EnhancedFind,
        Feature::WebUiFollowsTheme,
        Feature::LivePreview,
        Feature::TabShape,
        Feature::ReadingTheme,
    ];

    fn version(text: &str) -> FileVersion {
        FileVersion::parse(text).expect("parses")
    }

    fn all_on() -> Features {
        let mut features = Features::none();
        for feature in Feature::ALL {
            features.set(feature, true);
        }
        features
    }

    #[test]
    fn the_pinned_build_comes_from_offsets_h() {
        let from_offsets_h = format!(
            "{}.{}.{}.{}",
            shared::EDGE_VERSION_MAJOR,
            shared::EDGE_VERSION_MINOR,
            shared::EDGE_VERSION_BUILD,
            shared::EDGE_VERSION_REVISION
        );
        assert_eq!(from_offsets_h, pinned().to_string());
        assert_eq!("msedge.dll", shared::EDGE_MODULE);
    }

    #[test]
    fn only_the_dll_features_depend_on_the_build() {
        for feature in Feature::ALL {
            assert_eq!(DLL_FEATURES.contains(&feature), depends_on_pinned_build(feature));
        }
    }

    #[test]
    fn the_pinned_build_blocks_nothing_and_any_other_blocks_the_dll_features() {
        let pinned = version("155.0.4283.45");
        assert!(edge_support::paused(&all_on(), Some(pinned), pinned).is_empty());
        assert_eq!(
            DLL_FEATURES.to_vec(),
            edge_support::paused(&all_on(), Some(version("155.0.4300.2")), pinned)
        );
        assert_eq!(
            DLL_FEATURES.to_vec(),
            edge_support::paused(&all_on(), Some(version("154.0.4100.1")), pinned)
        );
    }

    #[test]
    fn features_already_off_are_not_reported() {
        let mut only_theming = Features::none();
        only_theming.theming = true;
        only_theming.quiet_dev_mode = true;
        let paused = edge_support::paused(
            &only_theming,
            Some(version("156.0.1.0")),
            version("155.0.4283.45"),
        );
        assert_eq!(vec![Feature::Theming], paused);
    }

    #[test]
    fn a_mismatched_process_decides_the_running_build() {
        let pinned = version("155.0.4283.45");
        let other = version("156.0.1.0");
        assert_eq!(None, judged(&[], pinned));
        assert_eq!(Some(pinned), judged(&[pinned, pinned], pinned));
        assert_eq!(Some(other), judged(&[pinned, other], pinned));
        assert_eq!(Some(other), judged(&[other], pinned));
    }
}
