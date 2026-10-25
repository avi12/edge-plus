//! The core's own plumbing: paths, settings, the log, and the actions the
//! window asks for.

pub mod action_error;
pub mod app_identity;
pub mod background_service;
pub mod panel_actions;
pub mod panel_log;
pub mod paths;
pub mod paused_toast;
pub mod poll_thread;
pub mod product;
pub mod settings;
pub mod switches;

#[path = "shared.g.rs"]
pub mod shared;
