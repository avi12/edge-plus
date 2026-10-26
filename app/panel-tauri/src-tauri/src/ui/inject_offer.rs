//! The window's Inject button, offered while auto-inject is off.

use crate::ui::panel_state::PanelState;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum InjectOffer {
    /// Not shown: auto-inject is on, Edge is closed, or no switch needs the DLL.
    #[default]
    Hidden,
    /// Shown and enabled: Edge is open without the DLL.
    Ready,
    /// Shown, disabled: the DLL went in after the button was offered. Stays
    /// until Edge restarts without it (Ready again) or the offer ends (Hidden).
    Injected,
}

/// `previous` is what the window showed before this state arrived.
#[must_use]
pub fn next(previous: InjectOffer, state: &PanelState) -> InjectOffer {
    let is_offered = !state.is_auto_inject && state.is_edge_running && state.is_split_wanted;
    if !is_offered {
        return InjectOffer::Hidden;
    }
    if !state.is_split_loaded {
        return InjectOffer::Ready;
    }
    if previous == InjectOffer::Hidden {
        return InjectOffer::Hidden;
    }
    InjectOffer::Injected
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::edge::version::file_version::FileVersion;
    use crate::platform::settings::{Features, Theming};

    const SUPPORTED: FileVersion = FileVersion::new(155, 0, 4283, 62);
    const EVERY_OFFER: [InjectOffer; 3] = [
        InjectOffer::Hidden,
        InjectOffer::Ready,
        InjectOffer::Injected,
    ];

    // Auto-inject off, Edge open on a supported build, a switch wanting the DLL.
    fn missing() -> PanelState {
        PanelState {
            features: Features::default(),
            theming: Theming::default(),
            edge_themes: Vec::new(),
            is_edge_running: true,
            is_start_with_windows: false,
            is_split_loaded: false,
            is_split_wanted: true,
            is_auto_inject: false,
            is_auto_update: true,
            edge_build: Some(SUPPORTED),
            supported_edge_build: SUPPORTED,
        }
    }

    fn loaded() -> PanelState {
        PanelState {
            is_split_loaded: true,
            ..missing()
        }
    }

    #[test]
    fn edge_open_without_the_dll_offers_the_button() {
        assert_eq!(InjectOffer::Ready, next(InjectOffer::Hidden, &missing()));
        assert_eq!(InjectOffer::Ready, next(InjectOffer::Ready, &missing()));
    }

    #[test]
    fn an_injection_shows_the_button_done_until_edge_restarts_without_it() {
        assert_eq!(InjectOffer::Injected, next(InjectOffer::Ready, &loaded()));
        assert_eq!(
            InjectOffer::Injected,
            next(InjectOffer::Injected, &loaded())
        );
        assert_eq!(InjectOffer::Ready, next(InjectOffer::Injected, &missing()));
    }

    #[test]
    fn auto_inject_edge_closed_or_nothing_wanted_hides_the_button() {
        for previous in EVERY_OFFER {
            for state in [
                PanelState {
                    is_auto_inject: true,
                    ..missing()
                },
                PanelState {
                    is_auto_inject: true,
                    ..loaded()
                },
                PanelState {
                    is_edge_running: false,
                    ..missing()
                },
                PanelState {
                    is_split_wanted: false,
                    ..missing()
                },
            ] {
                assert_eq!(InjectOffer::Hidden, next(previous, &state), "{previous:?}");
            }
        }
    }

    // The panel opened onto an Edge that already has the DLL: nothing to offer.
    #[test]
    fn a_dll_already_loaded_offers_nothing() {
        assert_eq!(InjectOffer::Hidden, next(InjectOffer::Hidden, &loaded()));
    }
}
