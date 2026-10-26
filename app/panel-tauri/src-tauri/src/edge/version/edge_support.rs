//! The pause is derived from the build every time and the user's choice is
//! never rewritten, so the choice comes back by itself. The check is the whole
//! build, not per feature: the DLL (verify.h) refuses anything but the exact
//! msedge.dll offsets.h was reversed against.

use crate::edge::version::edge_build;
use crate::edge::version::file_version::FileVersion;
use crate::platform::settings::{Feature, Features};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FeatureState {
    Off,
    On,
    /// Switched on, but held on an unsupported build; comes back by itself on
    /// a supported build.
    Paused,
    /// Switched off, and could not be switched on on this build.
    Unavailable,
}

/// An unknown build (Edge not installed or unreadable) holds nothing back.
#[must_use]
pub fn is_supported(edge: Option<FileVersion>, pinned: FileVersion) -> bool {
    edge.is_none_or(|build| build == pinned)
}

#[must_use]
pub fn state_of(
    choice: &Features,
    feature: Feature,
    edge: Option<FileVersion>,
    pinned: FileVersion,
) -> FeatureState {
    let is_on = choice.is_on(feature);
    let is_held = edge_build::depends_on_pinned_build(feature) && !is_supported(edge, pinned);
    match (is_held, is_on) {
        (false, true) => FeatureState::On,
        (false, false) => FeatureState::Off,
        (true, true) => FeatureState::Paused,
        (true, false) => FeatureState::Unavailable,
    }
}

#[must_use]
pub fn paused(choice: &Features, edge: Option<FileVersion>, pinned: FileVersion) -> Vec<Feature> {
    Feature::ALL
        .into_iter()
        .filter(|&feature| state_of(choice, feature, edge, pinned) == FeatureState::Paused)
        .collect()
}

/// What the DLL is sent, never what is saved.
#[must_use]
pub fn effective(choice: &Features, edge: Option<FileVersion>, pinned: FileVersion) -> Features {
    let mut effective = Features::none();
    for feature in Feature::ALL {
        effective.set(
            feature,
            state_of(choice, feature, edge, pinned) == FeatureState::On,
        );
    }
    effective
}

/// One toast per build that pauses something.
#[must_use]
pub fn should_notify(
    paused: &[Feature],
    edge: Option<FileVersion>,
    notified_for: Option<FileVersion>,
) -> bool {
    !paused.is_empty() && edge.is_some_and(|build| Some(build) != notified_for)
}

#[cfg(test)]
mod tests {
    use super::*;

    const PINNED: FileVersion = FileVersion::new(155, 0, 4283, 62);
    const UPDATE: FileVersion = FileVersion::new(156, 0, 1, 0);

    #[test]
    fn states_follow_the_choice_and_the_build() {
        let mut choice = Features::none();
        choice.native_split = true;
        assert_eq!(
            FeatureState::On,
            state_of(&choice, Feature::NativeSplit, Some(PINNED), PINNED)
        );
        assert_eq!(
            FeatureState::Paused,
            state_of(&choice, Feature::NativeSplit, Some(UPDATE), PINNED)
        );
        assert_eq!(
            FeatureState::Unavailable,
            state_of(&choice, Feature::Theming, Some(UPDATE), PINNED)
        );
        assert_eq!(
            FeatureState::Off,
            state_of(&choice, Feature::Theming, Some(PINNED), PINNED)
        );
        assert_eq!(
            FeatureState::On,
            state_of(&choice, Feature::NativeSplit, None, PINNED)
        );
        assert!(is_supported(Some(PINNED), PINNED));
        assert!(!is_supported(
            Some(FileVersion::new(154, 0, 4283, 62)),
            PINNED
        ));
    }

    #[test]
    fn the_choice_comes_back_on_a_supported_build() {
        let mut choice = Features::none();
        choice.native_split = true;
        choice.theming = true;
        choice.quiet_dev_mode = true;
        let held = effective(&choice, Some(UPDATE), PINNED);
        assert!(!held.native_split && !held.theming);
        assert!(held.quiet_dev_mode);
        assert!(choice.native_split && choice.theming);
        let back = effective(&choice, Some(PINNED), PINNED);
        assert!(back.native_split && back.theming);
    }

    #[test]
    fn only_dll_features_pause() {
        let mut all_on = Features::none();
        for feature in Feature::ALL {
            all_on.set(feature, true);
        }
        let held = paused(&all_on, Some(UPDATE), PINNED);
        assert!(!held.contains(&Feature::QuietDevMode));
        let dll_features = Feature::ALL
            .into_iter()
            .filter(|&feature| edge_build::depends_on_pinned_build(feature))
            .count();
        assert_eq!(dll_features, held.len());
        assert_eq!(Vec::<Feature>::new(), paused(&all_on, Some(PINNED), PINNED));
    }

    #[test]
    fn toast_once_per_build() {
        let some = [Feature::NativeSplit];
        assert!(should_notify(&some, Some(UPDATE), None));
        assert!(!should_notify(&some, Some(UPDATE), Some(UPDATE)));
        assert!(should_notify(
            &some,
            Some(FileVersion::new(156, 0, 2, 0)),
            Some(UPDATE)
        ));
        assert!(!should_notify(&[], Some(UPDATE), None));
    }
}
