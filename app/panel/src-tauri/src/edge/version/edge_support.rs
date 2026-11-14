//! The pause is derived from what the DLL says about the build every time and the
//! user's choice is never rewritten, so the choice comes back by itself.
//!
//! The build offsets.h is pinned to is trusted as it is. Any other build is not
//! turned away on its version: the DLL goes in, looks for the code it hooks by
//! signature and reports (`dll_status`) whether it could, and which features that
//! leaves. Only that report pauses a feature; until it arrives the build is
//! `Untried`, and a build the DLL stood down on is `Refused`.

use crate::dll::dll_status::{self, DllStatus, Mode};
use crate::edge::version::edge_build;
use crate::edge::version::file_version::FileVersion;
use crate::platform::settings::{Feature, Features};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum FeatureState {
    Off,
    On,
    /// Switched on, but held on a build the DLL cannot run it in; comes back by
    /// itself on a build it can.
    Paused,
    /// Switched off, and could not be switched on on this build.
    Unavailable,
}

/// What the panel knows about the DLL's chances in the running Edge build.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Support {
    /// The pinned build, or a build that cannot be read: nothing is held back.
    Full,
    /// Another build the DLL has not reported on yet: it is let in to try.
    Untried,
    /// Another build the DLL placed its hooks in by signature: only the features
    /// whose every address it found run (`dll_status::feature_bit`).
    Partial(u32),
    /// Another build the DLL stood down on.
    Refused,
}

/// `report` is the DLL's last word, which counts only if it is about `edge`.
#[must_use]
pub(crate) fn support_of(
    edge: Option<FileVersion>,
    pinned: FileVersion,
    report: Option<&DllStatus>,
) -> Support {
    let Some(build) = edge else {
        return Support::Full;
    };
    if build == pinned {
        return Support::Full;
    }
    match report.filter(|status| status.build == build) {
        Some(status) if status.mode == Mode::Resolved => {
            Support::Partial(status.available_features)
        }
        Some(status) if status.mode == Mode::Refused => Support::Refused,
        _ => Support::Untried,
    }
}

/// The support for the Edge that runs now, from the build and the DLL's word.
#[must_use]
pub(crate) fn support_now(edge: Option<FileVersion>) -> Support {
    support_of(edge, edge_build::pinned(), dll_status::read().as_ref())
}

/// Whether the DLL may be put into a browser process of this build: any build
/// but one it has already stood down on.
#[must_use]
pub(crate) fn may_inject(build: Option<FileVersion>) -> bool {
    support_now(build) != Support::Refused
}

#[must_use]
pub(crate) fn is_held(support: Support, feature: Feature) -> bool {
    if !edge_build::depends_on_pinned_build(feature) {
        return false;
    }
    match support {
        Support::Full | Support::Untried => false,
        Support::Partial(bits) => {
            dll_status::feature_bit(feature).is_none_or(|bit| bits & bit == 0)
        }
        Support::Refused => true,
    }
}

#[must_use]
pub(crate) fn state_of(choice: &Features, feature: Feature, support: Support) -> FeatureState {
    let is_on = choice.is_on(feature);
    match (is_held(support, feature), is_on) {
        (false, true) => FeatureState::On,
        (false, false) => FeatureState::Off,
        (true, true) => FeatureState::Paused,
        (true, false) => FeatureState::Unavailable,
    }
}

#[must_use]
pub(crate) fn paused(choice: &Features, support: Support) -> Vec<Feature> {
    Feature::ALL
        .into_iter()
        .filter(|&feature| state_of(choice, feature, support) == FeatureState::Paused)
        .collect()
}

/// What the DLL is sent, never what is saved.
#[must_use]
pub(crate) fn effective(choice: &Features, support: Support) -> Features {
    let mut effective = Features::none();
    for feature in Feature::ALL {
        effective.set(
            feature,
            state_of(choice, feature, support) == FeatureState::On,
        );
    }
    effective
}

/// One toast per build that pauses something.
#[must_use]
pub(crate) fn should_notify(
    paused: &[Feature],
    edge: Option<FileVersion>,
    notified_for: Option<FileVersion>,
) -> bool {
    !paused.is_empty() && edge.is_some_and(|build| Some(build) != notified_for)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::platform::shared;

    const PINNED: FileVersion = FileVersion::new(155, 0, 4283, 62);
    const UPDATE: FileVersion = FileVersion::new(156, 0, 1, 0);

    fn report(build: FileVersion, mode: Mode, available_features: u32) -> DllStatus {
        DllStatus {
            mode,
            build,
            available_features,
            resolved_entries: 0,
            total_entries: 0,
        }
    }

    #[test]
    fn the_pinned_build_and_an_unreadable_one_hold_nothing_back() {
        assert_eq!(Support::Full, support_of(Some(PINNED), PINNED, None));
        assert_eq!(Support::Full, support_of(None, PINNED, None));
        let refused = report(UPDATE, Mode::Refused, 0);
        assert_eq!(
            Support::Full,
            support_of(Some(PINNED), PINNED, Some(&refused)),
            "a word about another build does not touch the pinned one"
        );
    }

    #[test]
    fn another_build_is_tried_until_the_dll_says_otherwise() {
        assert_eq!(Support::Untried, support_of(Some(UPDATE), PINNED, None));
        let about_the_pinned = report(PINNED, Mode::Pinned, shared::FEATURE_BIT_SPLIT);
        assert_eq!(
            Support::Untried,
            support_of(Some(UPDATE), PINNED, Some(&about_the_pinned)),
            "a report about another build is no report"
        );
        let resolved = report(UPDATE, Mode::Resolved, shared::FEATURE_BIT_SPLIT);
        assert_eq!(
            Support::Partial(shared::FEATURE_BIT_SPLIT),
            support_of(Some(UPDATE), PINNED, Some(&resolved))
        );
        let refused = report(UPDATE, Mode::Refused, 0);
        assert_eq!(
            Support::Refused,
            support_of(Some(UPDATE), PINNED, Some(&refused))
        );
    }

    #[test]
    fn a_partial_build_pauses_only_what_the_dll_could_not_place() {
        let mut choice = Features::none();
        choice.native_split = true;
        choice.theming = true;
        choice.quiet_dev_mode = true;
        let support = Support::Partial(shared::FEATURE_BIT_SPLIT);
        assert_eq!(
            FeatureState::On,
            state_of(&choice, Feature::NativeSplit, support)
        );
        assert_eq!(
            FeatureState::Paused,
            state_of(&choice, Feature::Theming, support)
        );
        assert_eq!(
            FeatureState::Unavailable,
            state_of(&choice, Feature::CyberpunkSkin, support)
        );
        assert_eq!(
            FeatureState::On,
            state_of(&choice, Feature::QuietDevMode, support),
            "the developer-mode prompt never depended on the DLL"
        );
        assert_eq!(vec![Feature::Theming], paused(&choice, support));
        let sent = effective(&choice, support);
        assert!(sent.native_split && !sent.theming && sent.quiet_dev_mode);
    }

    #[test]
    fn a_refused_build_pauses_every_dll_feature_and_untried_none() {
        let mut all_on = Features::none();
        for feature in Feature::ALL {
            all_on.set(feature, true);
        }
        let held = paused(&all_on, Support::Refused);
        assert!(!held.contains(&Feature::QuietDevMode));
        let dll_features = Feature::ALL
            .into_iter()
            .filter(|&feature| edge_build::depends_on_pinned_build(feature))
            .count();
        assert_eq!(dll_features, held.len());
        for support in [Support::Full, Support::Untried] {
            assert_eq!(Vec::<Feature>::new(), paused(&all_on, support));
        }
    }

    #[test]
    fn the_choice_comes_back_when_the_dll_can_run() {
        let mut choice = Features::none();
        choice.native_split = true;
        choice.theming = true;
        choice.quiet_dev_mode = true;
        let held = effective(&choice, Support::Refused);
        assert!(!held.native_split && !held.theming);
        assert!(held.quiet_dev_mode);
        assert!(choice.native_split && choice.theming);
        let back = effective(&choice, Support::Full);
        assert!(back.native_split && back.theming);
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
