//! The stable program in the install root: the Start menu shortcut, the Add or
//! remove programs entry and the Run entry start it. It starts the newest
//! `app-x.y.z\edgeplus.exe` with the arguments it was given (`--background` at
//! sign-in, `--uninstall` from Add or remove programs), so none of them needs
//! to change when an update unpacks a new folder. An `--apply <pid>` command line
//! is an update handing over: it waits for that process to end, then starts the
//! newest version, or the one before it when that does not start.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::process::ExitCode;

use winui_svelte_launcher::{Application, ApplyRequest, LaunchFlags, Launcher};

const CORE_EXE: &str = "edgeplus.exe";

struct EdgePlus;

impl Application for EdgePlus {
    fn executable_name(&self) -> &str {
        CORE_EXE
    }
}

fn main() -> ExitCode {
    let Ok(executable) = std::env::current_exe() else {
        return ExitCode::FAILURE;
    };
    let Some(launcher) = Launcher::for_executable(EdgePlus, &executable) else {
        return ExitCode::FAILURE;
    };
    let arguments: Vec<String> = std::env::args().skip(1).collect();
    let is_ok = match ApplyRequest::parse(&arguments) {
        Some(request) => launcher.apply(&request).is_ok(),
        None => launcher.forward(&arguments, LaunchFlags::default()).is_ok(),
    };
    if is_ok {
        return ExitCode::SUCCESS;
    }
    ExitCode::FAILURE
}
