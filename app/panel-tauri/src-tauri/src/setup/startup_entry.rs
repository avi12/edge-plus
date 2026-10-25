//! The Run key is the one source of truth, never remembered elsewhere. Task
//! Manager's "Startup apps" switch leaves the Run value alone and writes a flag
//! under StartupApproved\Run, so that flag is honoured too.

use std::io;
use std::path::Path;

use edge_native::Hive;

use crate::platform::{paths, switches};

/// Approved holds Task Manager's on/off flag; tests point at a throwaway pair.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct StartupKeys {
    pub run: String,
    pub approved: String,
}

impl StartupKeys {
    #[must_use]
    pub fn user() -> Self {
        Self {
            run: r"Software\Microsoft\Windows\CurrentVersion\Run".to_owned(),
            approved: r"Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run"
                .to_owned(),
        }
    }
}

/// The earlier panels' name too, so `migrate_to` can take their entry over.
pub const NAME: &str = "Edge+";
/// Set in the first byte when Task Manager turned the entry off (0x02 / 0x06
/// on, 0x03 / 0x07 off).
const APPROVED_DISABLED_BIT: u8 = 1;
/// The Tauri panel's exe, from source and installed.
const TAURI_LAUNCHERS: [&str; 2] = ["fix-edge-panel.exe", "Edge+.exe"];
const QUOTE: char = '"';
const SPACE: char = ' ';

#[must_use]
pub fn run_command(executable: &Path) -> String {
    format!("\"{}\" {}", executable.display(), switches::BACKGROUND)
}

#[must_use]
pub fn is_disabled_flag(approved: Option<&[u8]>) -> bool {
    let first = approved.and_then(<[u8]>::first);
    first.is_some_and(|&flags| flags & APPROVED_DISABLED_BIT != 0)
}

#[must_use]
pub fn is_enabled(keys: &StartupKeys, name: &str) -> bool {
    let has_run_entry =
        edge_native::read_string(Hive::CurrentUser, &keys.run, Some(name)).is_some();
    if !has_run_entry {
        return false;
    }
    let approved = edge_native::read_binary(Hive::CurrentUser, &keys.approved, name);
    !is_disabled_flag(approved.as_deref())
}

/// Clears any Task Manager "disabled" flag, so the switch really means on.
pub fn enable(keys: &StartupKeys, name: &str, command: &str) -> io::Result<()> {
    edge_native::write_string(Hive::CurrentUser, &keys.run, name, command)?;
    edge_native::delete_value(Hive::CurrentUser, &keys.approved, name)
}

pub fn disable(keys: &StartupKeys, name: &str) -> io::Result<()> {
    edge_native::delete_value(Hive::CurrentUser, &keys.run, name)?;
    edge_native::delete_value(Hive::CurrentUser, &keys.approved, name)
}

/// Moves an entry an earlier panel (Tauri or C#) wrote over to
/// `executable`; true when it moved one.
pub fn migrate_to(executable: &Path) -> io::Result<bool> {
    migrate(&StartupKeys::user(), NAME, executable)
}

/// Keeps Task Manager's on/off flag as the user set it.
pub fn migrate(keys: &StartupKeys, name: &str, executable: &Path) -> io::Result<bool> {
    let Some(command) = edge_native::read_string(Hive::CurrentUser, &keys.run, Some(name)) else {
        return Ok(false);
    };
    if !is_tauri_command(&command) {
        return Ok(false);
    }
    edge_native::write_string(Hive::CurrentUser, &keys.run, name, &run_command(executable))?;
    Ok(true)
}

#[must_use]
pub fn is_tauri_command(command: &str) -> bool {
    let Some(launcher) = Path::new(launcher_of(command)).file_name() else {
        return false;
    };
    let launcher = paths::fold_case(&launcher.to_string_lossy());
    TAURI_LAUNCHERS
        .iter()
        .any(|&tauri| paths::fold_case(tauri) == launcher)
}

fn launcher_of(command: &str) -> &str {
    let trimmed = command.trim();
    if let Some(quoted) = trimmed.strip_prefix(QUOTE) {
        return quoted
            .find(QUOTE)
            .map_or(quoted, |closing_quote| &quoted[..closing_quote]);
    }
    trimmed
        .find(SPACE)
        .map_or(trimmed, |space| &trimmed[..space])
}

#[cfg(test)]
mod tests {
    use std::process::Command;

    use super::*;

    /// The registry checks write under a throwaway HKCU\Software\EdgePlusTest
    /// key, never the real Run key, and delete it afterwards.
    const TEST_ROOT: &str = r"Software\EdgePlusTest";
    const TEST_NAME: &str = "Edge+ test";

    struct TestKeys(StartupKeys);

    impl TestKeys {
        fn new() -> Self {
            cleanup();
            Self(StartupKeys {
                run: format!(r"{TEST_ROOT}\Run"),
                approved: format!(r"{TEST_ROOT}\Approved"),
            })
        }
    }

    impl Drop for TestKeys {
        fn drop(&mut self) {
            cleanup();
        }
    }

    fn cleanup() {
        let _ = edge_native::delete_tree(Hive::CurrentUser, TEST_ROOT);
    }

    /// Task Manager's binary flag; edge-native has no binary writer, so
    /// reg.exe writes it, under the test key only.
    fn set_approved(keys: &StartupKeys, flag: &str) {
        let status = Command::new("reg")
            .args([
                "add",
                &format!(r"HKCU\{}", keys.approved),
                "/v",
                TEST_NAME,
                "/t",
                "REG_BINARY",
                "/d",
                flag,
                "/f",
            ])
            .output()
            .expect("run reg.exe")
            .status;
        assert!(status.success(), "reg.exe wrote the flag");
    }

    #[test]
    fn the_command_quotes_a_path_with_spaces() {
        assert_eq!(
            "\"C:\\Program Files\\Edge+\\edgeplus.exe\" --background",
            run_command(Path::new(r"C:\Program Files\Edge+\edgeplus.exe"))
        );
    }

    #[test]
    fn reads_task_managers_flag() {
        assert!(is_disabled_flag(Some(&[0x03, 0, 0])), "0x03 is off");
        assert!(is_disabled_flag(Some(&[0x07])), "0x07 is off");
        assert!(!is_disabled_flag(Some(&[0x02, 0, 0])), "0x02 is on");
        assert!(!is_disabled_flag(Some(&[0x06])), "0x06 is on");
        assert!(!is_disabled_flag(Some(&[])), "an empty flag is on");
        assert!(!is_disabled_flag(None), "no flag is on");
    }

    #[test]
    fn tells_the_tauri_panel_apart() {
        let win_ui = Path::new(r"C:\Edge+\edgeplus.exe");
        assert!(
            is_tauri_command(
                "\"C:\\repos\\fix-edge\\target\\debug\\fix-edge-panel.exe\" --background"
            ),
            "the Tauri source build is the Tauri panel"
        );
        assert!(
            is_tauri_command(r"C:\Users\a\AppData\Local\Edge+\Edge+.exe"),
            "the installed Tauri panel, unquoted, is too"
        );
        assert!(!is_tauri_command(&run_command(win_ui)), "this panel is not");
    }

    /// Needs edge-native (the registry).
    #[test]
    fn the_run_entry_and_task_managers_flag() {
        let keys = TestKeys::new();
        let keys = &keys.0;
        assert!(!is_enabled(keys, TEST_NAME), "off before it is written");
        enable(keys, TEST_NAME, "x.exe --background").expect("enable");
        assert!(is_enabled(keys, TEST_NAME), "on once written");

        set_approved(keys, "030000000000000000000000");
        assert!(
            !is_enabled(keys, TEST_NAME),
            "Task Manager's flag shows off"
        );
        enable(keys, TEST_NAME, "x.exe --background").expect("enable");
        assert!(
            is_enabled(keys, TEST_NAME),
            "enabling clears Task Manager's flag"
        );

        disable(keys, TEST_NAME).expect("disable");
        assert!(!is_enabled(keys, TEST_NAME), "off once removed");
        disable(keys, TEST_NAME).expect("removing twice is not an error");
        assert!(!is_enabled(keys, TEST_NAME), "still off");

        migrates_only_a_tauri_entry(keys);
    }

    fn migrates_only_a_tauri_entry(keys: &StartupKeys) {
        let win_ui = Path::new(r"C:\Edge+\edgeplus.exe");
        assert!(
            !migrate(keys, TEST_NAME, win_ui).expect("migrate"),
            "no entry, nothing moved"
        );
        enable(
            keys,
            TEST_NAME,
            "\"D:\\x\\fix-edge-panel.exe\" --background",
        )
        .expect("enable");
        set_approved(keys, "03000000");
        assert!(
            migrate(keys, TEST_NAME, win_ui).expect("migrate"),
            "a Tauri entry is moved"
        );
        assert_eq!(
            Some(run_command(win_ui)),
            edge_native::read_string(Hive::CurrentUser, &keys.run, Some(TEST_NAME)),
            "it now starts this exe as the listener"
        );
        assert!(
            !is_enabled(keys, TEST_NAME),
            "Task Manager's off flag is kept through the move"
        );
        assert!(
            !migrate(keys, TEST_NAME, win_ui).expect("migrate"),
            "an entry for this exe is left alone"
        );
    }
}
