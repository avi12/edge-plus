//! Which Edge build is running, and which features it rules out.
//!
//! The split DLL is pinned to one `msedge.dll` build: every RVA and struct
//! offset in `app/native-split/src/offsets.h` belongs to it. build.rs reads that
//! build (and the module name) out of `offsets.h`, so the panel compares against
//! exactly what the embedded DLL was reversed against - there is no second copy
//! of the version to drift.
//!
//! What each feature depends on:
//!
//! * The split, the theming and asking to handle links all run inside the
//!   injected DLL and call into `msedge.dll` by RVA (`edge.h`; the link handler
//!   also rewrites a vtable slot), so a different `msedge.dll` breaks all three.
//! * The developer-mode prompt only edits a key in each profile's `Preferences`
//!   (`edge-native/src/devmode.rs`) and touches no Edge binary, so an update
//!   leaves it alone.

use edge_native::FileVersion;
use serde::Serialize;

use crate::commands;
use crate::settings::{Feature, Features, Settings};

/// The `msedge.dll` build the embedded DLL was made for (from `offsets.h`).
const PINNED_BUILD: &str = env!("FIXEDGE_PINNED_EDGE_BUILD");
/// The Edge module that build names (`kModule` in `offsets.h`).
const EDGE_MODULE: &str = env!("FIXEDGE_PINNED_EDGE_MODULE");

/// A newly seen Edge build, and what it switched off (often nothing).
#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct BuildChange {
    /// The build Edge is running now.
    pub edge_build: String,
    /// The build seen before this one, when there was one.
    pub previous_build: Option<String>,
    /// The only build the DLL supports.
    pub supported_build: String,
    /// The features that were switched off; empty when none had to be.
    pub disabled: Vec<Feature>,
}

/// The build the embedded DLL was made for.
#[must_use]
pub fn pinned() -> FileVersion {
    PINNED_BUILD
        .parse()
        .expect("build.rs exports offsets.h's four-part version")
}

/// Whether `feature` calls into `msedge.dll` and so only works on the pinned
/// build (see the module docs for why each one does or does not).
#[must_use]
pub fn depends_on_pinned_build(feature: Feature) -> bool {
    match feature {
        // The split, theming, the link handlers, the Gemini reskin and the
        // find bar all run inside the injected DLL and call into msedge.dll by
        // RVA, so a different build breaks them.
        Feature::NativeSplit
        | Feature::Theming
        | Feature::ProtocolHandlers
        | Feature::Assistant
        | Feature::EnhancedFind => true,
        Feature::QuietDevMode => false,
    }
}

/// The `msedge.dll` build browser process `pid` loaded, or `None` when it
/// cannot be read.
#[must_use]
pub fn build_of(pid: u32) -> Option<FileVersion> {
    edge_native::module_path(pid, EDGE_MODULE).and_then(|path| edge_native::file_version(&path))
}

/// The build running Edge is judged by: any build other than the pinned one if
/// a browser process runs one (the DLL must not go into it), else the pinned
/// one; `None` when no build could be read (Edge is not running).
#[must_use]
pub fn running_build() -> Option<FileVersion> {
    let builds: Vec<FileVersion> = edge_native::edge_browser_pids()
        .into_iter()
        .filter_map(build_of)
        .collect();
    judged_build(&builds, pinned())
}

fn judged_build(builds: &[FileVersion], pinned: FileVersion) -> Option<FileVersion> {
    let first = *builds.first()?;
    let other = builds.iter().copied().find(|&build| build != pinned);
    Some(other.unwrap_or(first))
}

/// The features that cannot run on `running` and are on in `features`.
#[must_use]
pub fn blocked_features(
    features: Features,
    running: FileVersion,
    pinned: FileVersion,
) -> Vec<Feature> {
    unsupported_features(running, pinned)
        .into_iter()
        .filter(|&feature| features.is_on(feature))
        .collect()
}

/// The features that cannot run on `running`, on or off.
#[must_use]
pub fn unsupported_features(running: FileVersion, pinned: FileVersion) -> Vec<Feature> {
    if running == pinned {
        return Vec::new();
    }
    Feature::ALL
        .into_iter()
        .filter(|&feature| depends_on_pinned_build(feature))
        .collect()
}

/// Look at the Edge that is running: remember its build, and when it is not
/// the pinned one switch off (and persist) every feature that depends on it,
/// before anything is injected. Returns the change when the build is a new one
/// or something was switched off, so the UI can re-read its state.
pub fn reconcile() -> Option<BuildChange> {
    let running = running_build()?;
    let pinned = pinned();
    let mut settings = Settings::load();
    let previous_build = settings.last_edge_build.clone();
    let disabled = blocked_features(settings.features, running, pinned);
    let is_new_build = previous_build.as_deref() != Some(running.to_string().as_str());
    let is_already_handled = disabled.is_empty() && !is_new_build;
    if is_already_handled {
        return None;
    }
    for &feature in &disabled {
        settings.features.set(feature, false);
    }
    settings.last_edge_build = Some(running.to_string());
    let _ = settings.save();
    if !disabled.is_empty() {
        // Write the switched-off state to the DLL's config and unload any copy
        // still loaded, so nothing is left pointed at the wrong build.
        commands::apply(&settings);
    }
    Some(BuildChange {
        edge_build: running.to_string(),
        previous_build,
        supported_build: pinned.to_string(),
        disabled,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn version(text: &str) -> FileVersion {
        text.parse().expect("valid version")
    }

    fn all_on() -> Features {
        Features {
            native_split: true,
            theming: true,
            quiet_dev_mode: true,
            protocol_handlers: true,
            assistant: crate::settings::Assistant::Gemini,
            enhanced_find: true,
        }
    }

    #[test]
    fn the_pinned_build_comes_from_offsets_h() {
        // build.rs failing to read offsets.h would not compile; this checks the
        // value it exported is a real four-part version.
        assert_eq!(pinned().to_string(), PINNED_BUILD);
        assert_eq!(EDGE_MODULE, "msedge.dll");
    }

    #[test]
    fn only_the_dll_features_depend_on_the_build() {
        assert!(depends_on_pinned_build(Feature::NativeSplit));
        assert!(depends_on_pinned_build(Feature::Theming));
        assert!(depends_on_pinned_build(Feature::ProtocolHandlers));
        assert!(depends_on_pinned_build(Feature::Assistant));
        assert!(depends_on_pinned_build(Feature::EnhancedFind));
        assert!(!depends_on_pinned_build(Feature::QuietDevMode));
    }

    #[test]
    fn the_pinned_build_blocks_nothing() {
        let pinned = version("155.0.4283.45");
        assert_eq!(blocked_features(all_on(), pinned, pinned), Vec::new());
        assert_eq!(unsupported_features(pinned, pinned), Vec::new());
    }

    #[test]
    fn an_update_blocks_the_dll_features_only() {
        let pinned = version("155.0.4283.45");
        let updated = version("155.0.4300.2");
        assert_eq!(
            blocked_features(all_on(), updated, pinned),
            vec![
                Feature::NativeSplit,
                Feature::Theming,
                Feature::ProtocolHandlers,
                Feature::Assistant,
                Feature::EnhancedFind
            ]
        );
    }

    #[test]
    fn a_downgrade_is_a_mismatch_too() {
        let pinned = version("155.0.4283.45");
        let older = version("154.0.4100.1");
        assert_eq!(
            unsupported_features(older, pinned),
            vec![
                Feature::NativeSplit,
                Feature::Theming,
                Feature::ProtocolHandlers,
                Feature::Assistant,
                Feature::EnhancedFind
            ]
        );
    }

    #[test]
    fn features_already_off_are_not_reported() {
        let pinned = version("155.0.4283.45");
        let updated = version("156.0.1.0");
        let only_theming = Features {
            native_split: false,
            theming: true,
            quiet_dev_mode: true,
            protocol_handlers: false,
            assistant: crate::settings::Assistant::Copilot,
            enhanced_find: false,
        };
        assert_eq!(
            blocked_features(only_theming, updated, pinned),
            vec![Feature::Theming]
        );
    }

    #[test]
    fn a_mismatched_process_decides_the_running_build() {
        let pinned = version("155.0.4283.45");
        let other = version("156.0.1.0");
        assert_eq!(judged_build(&[], pinned), None);
        assert_eq!(judged_build(&[pinned, pinned], pinned), Some(pinned));
        assert_eq!(judged_build(&[pinned, other], pinned), Some(other));
        assert_eq!(judged_build(&[other], pinned), Some(other));
    }
}
