//! Written once in Cargo.toml and read back at compile time, so the exe and the
//! window never disagree.

pub(crate) const NAME: &str = "Edge+";
pub(crate) const VERSION: &str = env!("CARGO_PKG_VERSION");
pub(crate) const AUTHOR: &str = env!("CARGO_PKG_AUTHORS");
pub(crate) const AUTHOR_URI: &str = "https://avi12.com";
