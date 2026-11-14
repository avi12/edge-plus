//! The exe's command-line switches.

include!("background_switch.rs");

/// Re-run headless as the dev-mode waiter (`dev_mode_prompt`).
pub(crate) const DEV_MODE_WAITER: &str = "--quiet-dev-mode-after-exit";

#[must_use]
pub(crate) fn is_present(command_line: &[String], name: &str) -> bool {
    command_line.iter().any(|argument| argument == name)
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
    fn finds_a_switch_among_the_arguments() {
        let command_line = line(&["edgeplus.exe", BACKGROUND]);
        assert!(is_present(&command_line, BACKGROUND));
        assert!(!is_present(&command_line, DEV_MODE_WAITER));
    }
}
