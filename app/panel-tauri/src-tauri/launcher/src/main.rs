//! The package's startup task runs this, since a task cannot pass arguments:
//! it starts the Edge+ exe beside it as a boot launch (no window) and ends.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::path::PathBuf;
use std::process::{Command, ExitCode};

/// The core's `switches::BACKGROUND`; the core's tests check the two agree.
const BACKGROUND_SWITCH: &str = "--background";
const CORE_EXE: &str = "edgeplus.exe";

fn core_beside_launcher() -> Option<PathBuf> {
    let launcher = std::env::current_exe().ok()?;
    Some(launcher.with_file_name(CORE_EXE))
}

fn main() -> ExitCode {
    let Some(core) = core_beside_launcher() else {
        return ExitCode::FAILURE;
    };
    match Command::new(core).arg(BACKGROUND_SWITCH).spawn() {
        Ok(_) => ExitCode::SUCCESS,
        Err(_) => ExitCode::FAILURE,
    }
}
