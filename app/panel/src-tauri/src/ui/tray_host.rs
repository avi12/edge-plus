//! The notification-area icon: a process of its own (ui/tray/tray.ts, the same
//! `winui-svelte` tray `AnySTT` uses), started with the core and ended with it,
//! since its stdin closes when the core goes. It draws the icon and its menu and
//! sends back which line was picked, one JSON frame per line.

use std::io::{BufRead, BufReader};
use std::os::windows::process::CommandExt;
use std::process::{Command, Stdio};
use std::thread;

use crate::platform::actions::panel_log;
use crate::ui::protocol::{RequestFrame, TrayMessage};
use crate::ui::window_kit::{self, Entry};

/// Keeps Node's console from flashing up.
const CREATE_NO_WINDOW: u32 = 0x0800_0000;

/// Starts the icon. `on_pick` runs on the tray's reader thread for each line
/// the user picks.
pub(crate) fn start(on_pick: impl Fn(TrayMessage) + Send + 'static) {
    let Some(command) = window_kit::locate(Entry::Tray) else {
        panel_log::write("the tray's script is missing; no icon shown");
        return;
    };
    let spawned = Command::new(&command.program)
        .args(&command.arguments)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::null())
        .creation_flags(CREATE_NO_WINDOW)
        .spawn();
    let mut child = match spawned {
        Ok(child) => child,
        Err(error) => {
            panel_log::write_error("starting the tray failed", &error);
            return;
        }
    };
    let (Some(stdin), Some(stdout)) = (child.stdin.take(), child.stdout.take()) else {
        return;
    };
    thread::Builder::new()
        .name("tray".to_owned())
        .spawn(move || {
            // Held for as long as the core lives: closing it ends the icon.
            let _link = stdin;
            for line in BufReader::new(stdout).lines().map_while(Result::ok) {
                match serde_json::from_str::<RequestFrame<TrayMessage>>(&line) {
                    Ok(frame) => on_pick(frame.message),
                    Err(error) => panel_log::write_error("a tray line was not understood", &error),
                }
            }
            let _ = child.wait();
        })
        .ok();
}
