//! The last word on a theme is Edge's own: `msedge --pack-extension` runs the
//! extension loader Edge installs with, so what it refuses Edge would refuse.
//! It runs in a throwaway profile, so the user's Edge is not touched. When
//! Edge cannot be run, the checks in `theme_manifest` have already passed and
//! stand alone.

use std::fs;
use std::io::{Cursor, Read};
use std::os::windows::process::CommandExt;
use std::path::Path;
use std::process::{Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

use zip::ZipArchive;

use crate::edge::version::edge_install;
use crate::platform::actions::panel_log;
use crate::themes::package::theme_manifest::{ThemeFileResult, invalid};
use crate::ui::wording;

const WORKSPACE_PREFIX: &str = "edgeplus-theme-check-";
const DOCUMENTATION_POINTER: &str = " See ";
const THEME_FOLDER: &str = "theme";
const PROFILE_FOLDER: &str = "profile";
const CHECK_TIMEOUT: Duration = Duration::from_secs(15);
const POLL_INTERVAL: Duration = Duration::from_millis(50);
const CREATE_NO_WINDOW: u32 = 0x0800_0000;
const ERROR_MARKER: &str = "ERROR:";
const LOG_PREFIX_END: &str = "] ";

/// Edge's refusal of the theme, worded for the user; Ok when Edge accepts it
/// and when it cannot be asked.
pub(crate) fn check(zip_bytes: &[u8]) -> ThemeFileResult<()> {
    let launcher = edge_install::application_directory()
        .filter(|_| !edge_install::is_simulated())
        .map(|directory| directory.join(edge_install::LAUNCHER_NAME))
        .filter(|launcher| launcher.is_file());
    let Some(launcher) = launcher else {
        return Ok(());
    };
    let workspace = std::env::temp_dir().join(format!("{WORKSPACE_PREFIX}{}", std::process::id()));
    let outcome = run(&launcher, &workspace, zip_bytes);
    let _ = fs::remove_dir_all(&workspace);
    match outcome {
        Ok(Some(reason)) => Err(invalid(wording::edge_rejected_theme(&reason))),
        Ok(None) => Ok(()),
        Err(error) => {
            panel_log::write_error("asking Edge to check a theme", &error);
            Ok(())
        }
    }
}

fn run(launcher: &Path, workspace: &Path, zip_bytes: &[u8]) -> std::io::Result<Option<String>> {
    let theme = workspace.join(THEME_FOLDER);
    fs::create_dir_all(&theme)?;
    ZipArchive::new(Cursor::new(zip_bytes))
        .and_then(|mut archive| archive.extract(&theme))
        .map_err(std::io::Error::other)?;
    let mut child = Command::new(launcher)
        .arg(format!("--pack-extension={}", theme.display()))
        .arg(format!(
            "--user-data-dir={}",
            workspace.join(PROFILE_FOLDER).display()
        ))
        .args(["--enable-logging=stderr", "--no-first-run"])
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::piped())
        .creation_flags(CREATE_NO_WINDOW)
        .spawn()?;
    let started = Instant::now();
    loop {
        if child.try_wait()?.is_some() {
            break;
        }
        if started.elapsed() > CHECK_TIMEOUT {
            // Only the process started here is stopped.
            child.kill()?;
            return Ok(None);
        }
        thread::sleep(POLL_INTERVAL);
    }
    let mut log = String::new();
    if let Some(mut stderr) = child.stderr.take() {
        stderr.read_to_string(&mut log)?;
    }
    Ok(refusal_in(&log))
}

/// The first error line of Edge's log, as a sentence for the user: without the
/// `[pid:tid:time:ERROR:file]` prefix and the documentation link Chromium
/// appends ("See developer.chrome.com/... for details.").
fn refusal_in(log: &str) -> Option<String> {
    log.lines()
        .filter(|line| line.contains(ERROR_MARKER))
        .find_map(|line| line.split_once(LOG_PREFIX_END))
        .map(|(_, reason)| {
            reason
                .split(DOCUMENTATION_POINTER)
                .next()
                .unwrap_or(reason)
                .trim()
                .to_owned()
        })
        .filter(|reason| !reason.is_empty())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::themes::package::theme_manifest::{MANIFEST_NAME, tests::zip_of};

    #[test]
    fn the_first_error_line_is_the_refusal() {
        let log = "[1:2:1110/224741.772:WARNING:extensions/common/extension.cc:762] Failed to load extension.
                   [1:2:1110/224741.773:ERROR:chrome/app/chrome_main_delegate.cc:706] Missing 'manifest_version' key. Its value must be an integer either 2 or 3. See developer.chrome.com/extensions/manifestVersion for details.
";
        assert_eq!(
            Some(
                "Missing 'manifest_version' key. Its value must be an integer either 2 or 3."
                    .to_owned()
            ),
            refusal_in(log)
        );
        assert_eq!(
            None,
            refusal_in(
                "[1:2:3:WARNING:x.cc:1] fine
"
            )
        );
        assert_eq!(None, refusal_in(""));
    }

    /// Runs the installed Edge: `cargo test -- --ignored`.
    #[test]
    #[ignore = "starts the installed msedge.exe"]
    fn edge_refuses_a_theme_it_cannot_load() {
        let missing_image = zip_of(&[(
            MANIFEST_NAME,
            br#"{"name":"t","manifest_version":3,"version":"1","theme":{"images":{"theme_frame":"gone.png"}}}"#,
        )]);
        let good = zip_of(&[(
            MANIFEST_NAME,
            br#"{"name":"t","manifest_version":3,"version":"1","theme":{}}"#,
        )]);
        let refusal = check(&missing_image).expect_err("refused");
        assert_eq!(
            "Edge refused the theme: Couldn't load gone.png for theme.",
            refusal.to_string()
        );
        assert!(check(&good).is_ok());
    }
}
