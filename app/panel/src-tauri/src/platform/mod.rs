//! The core's own plumbing: paths, settings, the log, and the actions the
//! window asks for.

pub(crate) mod actions;
pub(crate) mod identity;
pub(crate) mod paths;
pub(crate) mod service;
pub(crate) mod settings;

#[path = "shared.g.rs"]
pub(crate) mod shared;
