//! The core's own plumbing: paths, settings, the log, and the actions the
//! window asks for.

pub mod actions;
pub mod identity;
pub mod paths;
pub mod service;
pub mod settings;

#[path = "shared.g.rs"]
pub mod shared;
