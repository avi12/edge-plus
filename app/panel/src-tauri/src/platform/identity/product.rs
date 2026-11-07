//! Written once in Cargo.toml and read back at compile time, so the exe and the
//! window never disagree.

pub const NAME: &str = "Edge+";
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
pub const AUTHOR: &str = env!("CARGO_PKG_AUTHORS");
pub const AUTHOR_URI: &str = "https://avi12.com";
