//! Where a failure's details go, so the window only ever shows a plain
//! sentence (wording), never a raw error message.

use std::fmt::Display;
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::PathBuf;

use crate::platform::paths;

const FILE_NAME: &str = "panel.log";

#[must_use]
pub fn path() -> PathBuf {
    paths::data_root().join(FILE_NAME)
}

pub fn write_error(what: &str, error: &impl Display) {
    write(&format!("{what}: {error}"));
}

/// A log that cannot be written is no reason to fail what is being logged.
pub fn write(line: &str) {
    let _ = fs::create_dir_all(paths::data_root());
    let Ok(mut file) = OpenOptions::new().create(true).append(true).open(path()) else {
        return;
    };
    let _ = writeln!(file, "{} {line}", edge_native::local_timestamp());
}
