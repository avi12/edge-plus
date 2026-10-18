//! Starting Edge+ when Windows starts. The registry work is in `edge-native`;
//! this module decides what the Run entry is called and what it runs.

use std::path::Path;

/// The Run entry's name, as Task Manager's "Startup apps" lists it.
pub const STARTUP_ENTRY_NAME: &str = "Edge+";

/// The switch a boot launch carries so the panel starts minimized.
pub const BACKGROUND_ARG: &str = "--background";

/// The command line Windows runs at sign-in: the quoted panel path, then the
/// background switch.
pub fn run_command(executable: &Path) -> String {
    format!("\"{}\" {BACKGROUND_ARG}", executable.display())
}

/// Whether this launch came from the Run entry.
pub fn is_background_launch(args: impl IntoIterator<Item = String>) -> bool {
    args.into_iter().any(|arg| arg == BACKGROUND_ARG)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn command_quotes_a_path_with_spaces() {
        let command = run_command(Path::new(r"C:\Program Files\Edge+\Edge+.exe"));
        assert_eq!(
            command,
            r#""C:\Program Files\Edge+\Edge+.exe" --background"#
        );
    }

    #[test]
    fn background_switch_is_found_among_args() {
        let args = ["app.exe", "--background"].map(str::to_owned);
        assert!(is_background_launch(args));
        assert!(!is_background_launch(["app.exe".to_owned()]));
    }
}
