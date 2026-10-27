//! The window: its process (`window_host`), the state it is sent (view,
//! protocol) and the sentences the core says itself (wording). The window's
//! own text is in its markup.

pub mod inject_offer;
pub mod panel_layout;
pub mod panel_state;
pub mod protocol;
pub mod view;
pub mod window_host;
pub mod window_kit;
pub mod wording;
