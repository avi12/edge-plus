//! The exe's command-line switches.

/// A boot launch from the Run entry: no window.
pub const BACKGROUND: &str = "--background";

/// Re-run headless as the dev-mode waiter (`dev_mode_prompt`).
pub const DEV_MODE_WAITER: &str = "--quiet-dev-mode-after-exit";

/// The relaunch after an update, followed by the pid of the process it
/// replaces: the new one waits for it to exit before it looks for a running
/// panel to hand over to.
pub const RESTARTED_FROM: &str = "--restarted-from";

#[must_use]
pub fn is_present(command_line: &[String], name: &str) -> bool {
    command_line.iter().any(|argument| argument == name)
}

/// The argument after `name`, or None when the switch is absent or last.
#[must_use]
pub fn value_of<'line>(command_line: &'line [String], name: &str) -> Option<&'line str> {
    let index = command_line.iter().position(|argument| argument == name)?;
    command_line.get(index + 1).map(String::as_str)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn line(arguments: &[&str]) -> Vec<String> {
        arguments
            .iter()
            .map(|&argument| argument.to_owned())
            .collect()
    }

    #[test]
    fn reads_a_switch_and_its_value() {
        let command_line = line(&["edgeplus.exe", RESTARTED_FROM, "42", BACKGROUND]);
        assert!(is_present(&command_line, BACKGROUND));
        assert!(!is_present(&command_line, DEV_MODE_WAITER));
        assert_eq!(Some("42"), value_of(&command_line, RESTARTED_FROM));
    }

    #[test]
    fn a_switch_last_has_no_value() {
        let command_line = line(&["edgeplus.exe", RESTARTED_FROM]);
        assert_eq!(None, value_of(&command_line, RESTARTED_FROM));
        assert_eq!(None, value_of(&command_line, BACKGROUND));
    }
}
