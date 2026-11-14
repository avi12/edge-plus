//! The window: its process (`window_host`), the state it is sent (view,
//! protocol) and the sentences the core says itself (wording). The window's
//! own text is in its markup.

pub(crate) mod inject_offer;
pub(crate) mod panel_layout;
pub(crate) mod panel_state;
pub(crate) mod protocol;
pub(crate) mod view;
pub(crate) mod window_host;
pub(crate) mod window_kit;
pub(crate) mod wording;
